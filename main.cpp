/*
*
* Copyright (C) 2016 OtherCrashOverride@users.noreply.github.com.
* All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2, as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
*/


// Developer notes:
// g++ -g main.cpp -o c2cap -l vpcodec
// ffmpeg -framerate 60 -i test.h264 -vcodec copy test.mp4
// sudo apt install libjpeg-turbo8-dev
// echo 1 | sudo tee /sys/class/graphics/fb0/blank

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <cstdlib> //atoi

#include <exception>
#include <vector>
#include <cstring>

#include "../c2_vpcodec/include/vpcodec_1_0.h"

#include <linux/videodev2.h> // V4L
#include <sys/mman.h>	// mmap


#include <turbojpeg.h>
#include <memory>
#include <ctime>

#include "Exception.h"
#include "Stopwatch.h"
#include "Timer.h"
#include "Mutex.h"

// Ion video header from drivers\staging\android\uapi\ion.h
#include "ion.h"
#include "meson_ion.h"
#include "ge2d.h"
#include "ge2d_cmd.h"

#include "IonBuffer.h"
#include "FontData.h"



const char* DEFAULT_DEVICE = "/dev/video0";
const char* DEFAULT_OUTPUT = "default.h264";
const int BUFFER_COUNT = 4;

const int DEFAULT_WIDTH = 640;
const int DEFAULT_HEIGHT = 480;
const int DEFAULT_FRAME_RATE = 30;
const int DEFAULT_BITRATE = 1000000 * 5;



const size_t MJpegDhtLength = 0x1A4;
unsigned char MJpegDht[MJpegDhtLength] = {
	/* JPEG DHT Segment for YCrCb omitted from MJPG data */
	0xFF,0xC4,0x01,0xA2,
	0x00,0x00,0x01,0x05,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x01,0x00,0x03,0x01,0x01,0x01,0x01,
	0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
	0x08,0x09,0x0A,0x0B,0x10,0x00,0x02,0x01,0x03,0x03,0x02,0x04,0x03,0x05,0x05,0x04,0x04,0x00,
	0x00,0x01,0x7D,0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,
	0x07,0x22,0x71,0x14,0x32,0x81,0x91,0xA1,0x08,0x23,0x42,0xB1,0xC1,0x15,0x52,0xD1,0xF0,0x24,
	0x33,0x62,0x72,0x82,0x09,0x0A,0x16,0x17,0x18,0x19,0x1A,0x25,0x26,0x27,0x28,0x29,0x2A,0x34,
	0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x53,0x54,0x55,0x56,
	0x57,0x58,0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x73,0x74,0x75,0x76,0x77,0x78,
	0x79,0x7A,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,
	0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,
	0xBA,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,
	0xDA,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,
	0xF8,0xF9,0xFA,0x11,0x00,0x02,0x01,0x02,0x04,0x04,0x03,0x04,0x07,0x05,0x04,0x04,0x00,0x01,
	0x02,0x77,0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,
	0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,0xA1,0xB1,0xC1,0x09,0x23,0x33,0x52,0xF0,0x15,0x62,
	0x72,0xD1,0x0A,0x16,0x24,0x34,0xE1,0x25,0xF1,0x17,0x18,0x19,0x1A,0x26,0x27,0x28,0x29,0x2A,
	0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x53,0x54,0x55,0x56,
	0x57,0x58,0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x73,0x74,0x75,0x76,0x77,0x78,
	0x79,0x7A,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,
	0x99,0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,
	0xB9,0xBA,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,
	0xD9,0xDA,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,
	0xF9,0xFA
};


struct BufferMapping
{
	void* Start;
	size_t Length;
};


struct option longopts[] = {
	{ "device",			required_argument,	NULL,	'd' },
	{ "output",			required_argument,	NULL,	'o' },
	{ "width",			required_argument,	NULL,	'w' },
	{ "height",			required_argument,	NULL,	'h' },
	{ "fps",			required_argument,	NULL,	'f' },
	{ "bitrate",		required_argument,	NULL,	'b' },
	{ "pixformat",		required_argument,	NULL,	'p' },
	{ "timestamp",		no_argument,		NULL,	't' },
	{ 0, 0, 0, 0 }
};



vl_codec_handle_t handle;
int encoderFileDescriptor = -1;
unsigned char* encodeNV12Buffer = nullptr;
char* encodeBitstreamBuffer = nullptr;
size_t encodeBitstreamBufferLength = 0;
Mutex encodeMutex;
unsigned char* mjpegData = nullptr;
size_t mjpegDataLength = 0;
bool needsMJpegDht = true;
double timeStamp = 0;
double frameRate = 0;

void EncodeFrame()
{
	encodeMutex.Lock();

	// Encode the video frames
	vl_frame_type_t type = FRAME_TYPE_AUTO;
	char* in = (char*)encodeNV12Buffer;
	int in_size = encodeBitstreamBufferLength;
	char* out = encodeBitstreamBuffer;
	int outCnt = vl_video_encoder_encode(handle, type, in, in_size, &out);
	//printf("vl_video_encoder_encode = %d\n", outCnt);

	encodeMutex.Unlock();


	if (outCnt > 0)
	{
		ssize_t writeCount = write(encoderFileDescriptor, encodeBitstreamBuffer, outCnt);
		if (writeCount < 0)
		{
			throw Exception("write failed.");
		}
	}


	timeStamp += frameRate;
}



enum class PictureFormat
{
	Unknown = 0,
	Yuyv = V4L2_PIX_FMT_YUYV,
	MJpeg = V4L2_PIX_FMT_MJPEG
};



Stopwatch sw;
Timer timer;

//int ion_fd = -1;
int ge2d_fd = -1;
std::shared_ptr<IonBuffer> YuvSource;
std::shared_ptr<IonBuffer> YuvDestination;

std::shared_ptr<IonBuffer> TimeStampBuffer;
uint8_t* TimeStampMapping;

void TimeStamp(int width, int height)
{
	const int WIDTH = 28 * 8; //320; //640;
	const int HEIGHT = 16;

	if (!TimeStampBuffer)
	{
		TimeStampBuffer = std::make_shared<IonBuffer>(WIDTH * HEIGHT);

		TimeStampMapping = (uint8_t*)mmap(NULL,
			TimeStampBuffer->Length(),
			PROT_READ | PROT_WRITE,
			MAP_FILE | MAP_SHARED,
			TimeStampBuffer->ExportHandle(),
			0);
	}


	// Ridiculously over complicated way to get a timestamp

	// used for calendar date and hours, minutes
	time_t rawtime;
	struct tm* timeinfo;

	// used for microseconds
	timeval tv;
	gettimeofday(&tv, 0);

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	const int COLUMNS = WIDTH / 8;
	
	char text[COLUMNS];
	size_t length = strftime(text, (size_t)COLUMNS, "%a %F %H:%M", timeinfo);

	double seconds = timeinfo->tm_sec + (tv.tv_usec * 0.000001);

	char finalText[COLUMNS];
	snprintf(finalText, COLUMNS, " %s:%05.2f", text, seconds);

	//fprintf(stderr, "%s\n", finalText);


	// Clear
	for (size_t i = 0; i < TimeStampBuffer->Length() / 4; ++i)
	{
		((int*)TimeStampMapping)[i] = 0;
	}


	// Render text
	char* textPtr = finalText;
	int col = 0;
	while (true)
	{
		char c = *textPtr++;
		if (c == 0)
			break;

		if (c != ' ')
		{
			// Lookup descriptor
			int entry = c - dejaVuSansMono_8ptFontInfo.StartCharacter;
			//if (c >= '@')
			//{
			//	entry = c - '@' + 31;
			//}

			FONT_CHAR_INFO info = dejaVuSansMono_8ptDescriptors[entry];
			const uint8_t* bitmap = dejaVuSansMono_8ptBitmaps + (entry * 12); //info.Offset;


			for (int y = 0; y < 12; ++y)
			{
				uint8_t* dest = TimeStampMapping + ((y + 1) * WIDTH) + (col * 8);

				uint8_t sample = *(bitmap + y);

				//for (int j = 0; j < 4; ++j)
				{
					//uint8_t sample = bigSample >> (8 * j);
					//sample &= 0x000000ff;

					uint8_t scan[8];
					scan[0] = sample & 0x80 ? 0xff : 0x00;
					scan[1] = sample & 0x40 ? 0xff : 0x00;
					scan[2] = sample & 0x20 ? 0xff : 0x00;
					scan[3] = sample & 0x10 ? 0xff : 0x00;

					scan[4] = sample & 0x08 ? 0xff : 0x00;
					scan[5] = sample & 0x04 ? 0xff : 0x00;
					scan[6] = sample & 0x02 ? 0xff : 0x00;
					scan[7] = sample & 0x01 ? 0xff : 0x00;

					//for (int i = 0; i < 8; ++i)
					//{
					//	*dest++ = scan[i];
					//}
					*((uint32_t*)dest) = *((uint32_t*)scan);
					dest += 4;
					*((uint32_t*)dest) = *((uint32_t*)(scan + 4));
				}
			}
		}

		++col;
	}

	TimeStampBuffer->Sync();


	// blit
	config_para_s config = { 0 };

	//config.src_dst_type = ALLOC_OSD0; //ALLOC_ALLOC; //ALLOC_OSD0;
	config.src_dst_type = ALLOC_ALLOC;
	config.alu_const_color = 0xffffffff;

	config.src_format = GE2D_LITTLE_ENDIAN | GE2D_FORMAT_M24_RGB; //GE2D_FORMAT_S8_Y;
	config.src_planes[0].addr = TimeStampBuffer->PhysicalAddress();
	config.src_planes[0].w = WIDTH;
	config.src_planes[0].h = HEIGHT;
	config.src_planes[1].addr = TimeStampBuffer->PhysicalAddress();
	config.src_planes[1].w = WIDTH;
	config.src_planes[1].h = HEIGHT;
	config.src_planes[2].addr = TimeStampBuffer->PhysicalAddress();
	config.src_planes[2].w = WIDTH;
	config.src_planes[2].h = HEIGHT;

	//config.dst_format = GE2D_LITTLE_ENDIAN | GE2D_FORMAT_S32_ARGB; //GE2D_FORMAT_M24_NV21; //GE2D_FORMAT_S32_ARGB;
	
	config.dst_format = GE2D_LITTLE_ENDIAN | GE2D_FORMAT_M24_NV21; //GE2D_FORMAT_M24_NV21; //GE2D_FORMAT_S32_ARGB;
	config.dst_planes[0].addr = YuvDestination->PhysicalAddress();
	config.dst_planes[0].w = width;
	config.dst_planes[0].h = height;
	config.dst_planes[1].addr = config.dst_planes[0].addr + (width * height);
	config.dst_planes[1].w = width;
	config.dst_planes[1].h = height / 2;

	int io = ioctl(ge2d_fd, GE2D_CONFIG, &config);
	if (io < 0)
	{
		throw Exception("GE2D_CONFIG failed");
	}

	// Perform the blit operation
	ge2d_para_s blitRect = { 0 };

	blitRect.src1_rect.x = 0;
	blitRect.src1_rect.y = 0;
	blitRect.src1_rect.w = WIDTH;
	blitRect.src1_rect.h = HEIGHT;

	blitRect.dst_rect.x = 0;
	blitRect.dst_rect.y = 0;
	blitRect.dst_rect.w = WIDTH;
	blitRect.dst_rect.h = HEIGHT;

	io = ioctl(ge2d_fd, GE2D_STRETCHBLIT_NOALPHA, &blitRect);
	if (io < 0)
	{
		throw Exception("GE2D_BLIT_NOALPHA failed.");
	}

	
}

int main(int argc, char** argv)
{
	int io;


	// options
	const char* device = DEFAULT_DEVICE;
	const char* output = DEFAULT_OUTPUT;
	int width = DEFAULT_WIDTH;
	int height = DEFAULT_HEIGHT;
	int fps = DEFAULT_FRAME_RATE;
	int bitrate = DEFAULT_BITRATE;
	PictureFormat pixformat = PictureFormat::Yuyv;
	bool timeStamp = false;

	int c;
	while ((c = getopt_long(argc, argv, "d:o:w:h:f:b:p:t", longopts, NULL)) != -1)
	{
		switch (c)
		{
			case 'd':			
				device = optarg;			
				break;

			case 'o':
				output = optarg;
				break;

			case 'w':
				width = atoi(optarg);
				break;

			case 'h':
				height = atoi(optarg);
				break;

			case 'f':
				fps = atoi(optarg);
				break;

			case 'b':
				bitrate = atoi(optarg);
				break;

			case 'p':
				if (strcmp(optarg, "yuyv") == 0)
				{
					pixformat = PictureFormat::Yuyv;
				}
				else if (strcmp(optarg, "mjpeg") == 0)
				{
					pixformat = PictureFormat::MJpeg;
				}
				else
				{
					throw Exception("Unknown pixformat.");
				}
				break;

			case 't':
				timeStamp = true;
				break;

			default:
				throw Exception("Unknown option.");
		}
	}


	int captureDev = open(device, O_RDWR);
	if (captureDev < 0)
	{
		throw Exception("capture device open failed.");
	}


	v4l2_capability caps = { 0 };
	io = ioctl(captureDev, VIDIOC_QUERYCAP, &caps);
	if (io < 0)
	{
		throw Exception("VIDIOC_QUERYCAP failed.");
	}

	fprintf(stderr, "card = %s\n", (char*)caps.card);
	fprintf(stderr, "\tbus_info = %s\n", (char*)caps.bus_info);
	fprintf(stderr, "\tdriver = %s\n", (char*)caps.driver);

	if (!caps.capabilities & V4L2_CAP_VIDEO_CAPTURE)
	{
		throw Exception("V4L2_CAP_VIDEO_CAPTURE not supported.");
	}
	else
	{
		fprintf(stderr, "V4L2_CAP_VIDEO_CAPTURE supported.\n");
	}



	v4l2_fmtdesc formatDesc = { 0 };
	formatDesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	
	fprintf(stderr, "Supported formats:\n");
	while (true)
	{
		io = ioctl(captureDev, VIDIOC_ENUM_FMT, &formatDesc);
		if (io < 0)
		{
			//printf("VIDIOC_ENUM_FMT failed.\n");
			break;
		}
		
		fprintf(stderr, "\tdescription = %s, pixelformat=0x%x\n", formatDesc.description, formatDesc.pixelformat);

		
		v4l2_frmsizeenum formatSize = { 0 };
		formatSize.pixel_format = formatDesc.pixelformat;

		while (true)
		{
			io = ioctl(captureDev, VIDIOC_ENUM_FRAMESIZES, &formatSize);
			if (io < 0)
			{
				//printf("VIDIOC_ENUM_FRAMESIZES failed.\n");
				break;
			}

			fprintf(stderr, "\t\twidth = %d, height = %d\n", formatSize.discrete.width, formatSize.discrete.height);


			v4l2_frmivalenum frameInterval = { 0 };
			frameInterval.pixel_format = formatSize.pixel_format;
			frameInterval.width = formatSize.discrete.width;
			frameInterval.height = formatSize.discrete.height;

			while (true)
			{
				io = ioctl(captureDev, VIDIOC_ENUM_FRAMEINTERVALS, &frameInterval);
				if (io < 0)
				{
					//printf("VIDIOC_ENUM_FRAMEINTERVALS failed.\n");
					break;
				}

				fprintf(stderr, "\t\t\tnumerator = %d, denominator = %d\n", frameInterval.discrete.numerator, frameInterval.discrete.denominator);
				++frameInterval.index;
			}


			++formatSize.index;
		}

		++formatDesc.index;
	}

	
	// Apply capture settings
	v4l2_format format = { 0 };
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.width = width;
	format.fmt.pix.height = height;
	//format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	//format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
	format.fmt.pix.pixelformat = (__u32)pixformat;
	format.fmt.pix.field = V4L2_FIELD_ANY; //V4L2_FIELD_INTERLACED; //V4L2_FIELD_ANY; V4L2_FIELD_ALTERNATE

	io = ioctl(captureDev, VIDIOC_S_FMT, &format);
	if (io < 0)
	{
		throw Exception("VIDIOC_S_FMT failed.");
	}

	fprintf(stderr, "v4l2_format: width=%d, height=%d, pixelformat=0x%x\n",
		format.fmt.pix.width, format.fmt.pix.height, format.fmt.pix.pixelformat);

	// Readback device selected settings
	width = format.fmt.pix.width;
	height = format.fmt.pix.height;
	pixformat = (PictureFormat)format.fmt.pix.pixelformat;


	v4l2_streamparm streamParm = { 0 };
	streamParm.type = format.type;
	streamParm.parm.capture.timeperframe.numerator = 1;
	streamParm.parm.capture.timeperframe.denominator = fps;

	io = ioctl(captureDev, VIDIOC_S_PARM, &streamParm);
	if (io < 0)
	{
		throw Exception("VIDIOC_S_PARM failed.");
	}

	fprintf(stderr, "capture.timeperframe: numerator=%d, denominator=%d\n",
		streamParm.parm.capture.timeperframe.numerator,
		streamParm.parm.capture.timeperframe.denominator);

	// Note: Video is encoded at the requested framerate whether
	// the device produces it or not.  Therefore, there is no
	// need to read back the fps.


	// Request buffers
	v4l2_requestbuffers requestBuffers = { 0 };
	requestBuffers.count = BUFFER_COUNT;
	requestBuffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	requestBuffers.memory = V4L2_MEMORY_MMAP;

	io = ioctl(captureDev, VIDIOC_REQBUFS, &requestBuffers);
	if (io < 0)
	{
		throw Exception("VIDIOC_REQBUFS failed.");
	}

	
	// Map buffers
	BufferMapping bufferMappings[requestBuffers.count] = { 0 };
	for (int i = 0; i < requestBuffers.count; ++i)
	{
		v4l2_buffer buffer = { 0 };
		buffer.type = requestBuffers.type;
		buffer.memory = V4L2_MEMORY_MMAP;
		buffer.index = i;

		io = ioctl(captureDev, VIDIOC_QUERYBUF, &buffer);
		if (io < 0)
		{
			throw Exception("VIDIOC_QUERYBUF failed.");
		}

		bufferMappings[i].Length = buffer.length;
		bufferMappings[i].Start = mmap(NULL, buffer.length,
			PROT_READ | PROT_WRITE, /* recommended */
			MAP_SHARED,             /* recommended */
			captureDev, buffer.m.offset);
	}


	// Queue buffers
	for (int i = 0; i < requestBuffers.count; ++i)
	{
		v4l2_buffer buffer = { 0 };
		buffer.index = i;
		buffer.type = requestBuffers.type;
		buffer.memory = requestBuffers.memory;

		io = ioctl(captureDev, VIDIOC_QBUF, &buffer);
		if (io < 0)
		{
			throw Exception("VIDIOC_QBUF failed.");
		}
	}




	// Create an output file	
	int fdOut;
	
	if (std::strcmp(output, "-") == 0)
	{
		fdOut = 1; //stdout
	}
	else
	{
		mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
		
		fdOut = open(output, O_CREAT | O_TRUNC | O_WRONLY, mode);
		if (fdOut < 0)
		{
			throw Exception("open output failed.");
		}
	}

	encoderFileDescriptor = fdOut;


	
	// GE2D
	ge2d_fd = open("/dev/ge2d", O_RDWR);
	if (ge2d_fd < 0)
	{
		throw Exception("open /dev/ge2d failed.");
	}



	// Ion
	YuvSource = std::make_shared<IonBuffer>(width * height * 4);
	
	void* yuvSourcePtr = mmap(NULL,
		YuvSource->Length(),
		PROT_READ | PROT_WRITE,
		MAP_FILE | MAP_SHARED,
		YuvSource->ExportHandle(),
		0);
	if (!yuvSourcePtr)
	{
		throw Exception("YuvSource mmap failed.");
	}


	YuvDestination = std::make_shared<IonBuffer>(width * height * 4);

	void* yuvDestinationPtr = mmap(NULL,
		YuvDestination->Length(),
		PROT_READ | PROT_WRITE,
		MAP_FILE | MAP_SHARED,
		YuvDestination->ExportHandle(),
		0);
	if (!yuvDestinationPtr)
	{
		throw Exception("YuvDestination mmap failed.");
	}




	// Initialize the encoder
	vl_codec_id_t codec_id = CODEC_ID_H264;
	width = format.fmt.pix.width;
	height = format.fmt.pix.height;
	fps = (int)((double)streamParm.parm.capture.timeperframe.denominator /
				(double)streamParm.parm.capture.timeperframe.numerator);
	//int bit_rate = bitrate;
	const int gop = 10;

	fprintf(stderr, "vl_video_encoder_init: width=%d, height=%d, fps=%d, bitrate=%d, gop=%d\n",
		width, height, fps, bitrate, gop);

	vl_img_format_t img_format = IMG_FMT_NV12;
	handle = vl_video_encoder_init(codec_id, width, height, fps, bitrate, gop, img_format);
	fprintf(stderr, "handle = %ld\n", handle);



	// Start streaming
	int bufferType = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	io = ioctl(captureDev, VIDIOC_STREAMON, &bufferType);
	if (io < 0)
	{
		throw Exception("VIDIOC_STREAMON failed.");
	}


	int nv12Size = format.fmt.pix.width * format.fmt.pix.height * 4;
	unsigned char* nv12 = (unsigned char*)yuvDestinationPtr; //new unsigned char[nv12Size];
	encodeNV12Buffer = nv12;
	fprintf(stderr, "nv12Size = %d\n", nv12Size);

	int ENCODE_BUFFER_SIZE = nv12Size; //1024 * 32;
	char* encodeBuffer = new char[ENCODE_BUFFER_SIZE];
	encodeBitstreamBuffer = encodeBuffer;
	//fprintf(stderr, "ENCODEC_BUFFER_SIZE = %d\n", ENCODEC_BUFFER_SIZE);


	// jpeg-turbo
	tjhandle jpegDecompressor = tjInitDecompress();
	int jpegWidth = 0;
	int jpegHeight = 0;
	int jpegSubsamp = 0;
	int jpegColorspace = 0;
	unsigned char* jpegYuv = nullptr;
	unsigned long jpegYuvSize = 0;


	bool isFirstFrame = true;
	/*bool needsMJpegDht = true;*/
	int frames = 0;
	float totalTime = 0;
	float lastTimestamp = 0;
	sw.Start(); //ResetTime();
	
	timer.Callback = EncodeFrame; //EncodeFrameHardware
	timer.SetInterval(1.0 / fps);
	//timer.Start();

	while (true)
	{
		// get buffer
		v4l2_buffer buffer = { 0 };
		buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buffer.memory = V4L2_MEMORY_MMAP;

		io = ioctl(captureDev, VIDIOC_DQBUF, &buffer);
		if (io < 0)
		{
			throw Exception("VIDIOC_DQBUF failed.");
		}


		if (pixformat == PictureFormat::Yuyv)
		{
			if (isFirstFrame)
			{
				frameRate = timer.Interval();
				timer.Start();

				isFirstFrame = false;
			}


			// Process frame
			encodeMutex.Lock();


			//printf("Got a buffer: index = %d\n", buffer.index);


			// Blit
			unsigned char* data = (unsigned char*)bufferMappings[buffer.index].Start;
			size_t dataLength = buffer.bytesused;

#if 0
			memcpy((void*)yuvSourcePtr, data, dataLength);
#else
			unsigned char* dest = (unsigned char*)yuvSourcePtr;

			for (size_t i = 0; i < dataLength; i += 4)
			{
				dest[i] = data[i + 1];
				dest[i + 1] = data[i + 0];
				dest[i + 2] = data[i + 3];
				dest[i + 3] = data[i + 2];
			}
#endif


			YuvSource->Sync();


			// Configure GE2D

			config_para_s config = { 0 };

			config.src_dst_type = ALLOC_ALLOC; //; //ALLOC_OSD0;
			config.alu_const_color = 0xffffffff;
			
			config.src_format = GE2D_LITTLE_ENDIAN | GE2D_FMT_S16_YUV422;
			config.src_planes[0].addr = YuvSource->PhysicalAddress();
			config.src_planes[0].w = width;
			config.src_planes[0].h = height;

			config.dst_format = GE2D_LITTLE_ENDIAN | GE2D_FORMAT_M24_NV21; //; //GE2D_FORMAT_S32_ARGB;
			config.dst_planes[0].addr = YuvDestination->PhysicalAddress();
			config.dst_planes[0].w = width;
			config.dst_planes[0].h = height;
			config.dst_planes[1].addr = config.dst_planes[0].addr + (format.fmt.pix.width * format.fmt.pix.height);
			config.dst_planes[1].w = width;
			config.dst_planes[1].h = height / 2;

			io = ioctl(ge2d_fd, GE2D_CONFIG, &config);
			if (io < 0)
			{
				throw Exception("GE2D_CONFIG failed");
			}


			// Perform the blit operation
			ge2d_para_s blitRect = { 0 };

			blitRect.src1_rect.x = 0;
			blitRect.src1_rect.y = 0;
			blitRect.src1_rect.w = width;
			blitRect.src1_rect.h = height;

			blitRect.dst_rect.x = 0;
			blitRect.dst_rect.y = 0;
			blitRect.dst_rect.w = width;
			blitRect.dst_rect.h = height;

			io = ioctl(ge2d_fd, GE2D_BLIT_NOALPHA, &blitRect);
			if (io < 0)
			{
				throw Exception("GE2D_BLIT_NOALPHA failed.");
			}

			//printf("GE2D Blit OK.\n");

			if (timeStamp)
			{
				TimeStamp(width, height);
			}

			YuvDestination->Sync();

			encodeMutex.Unlock();
		}
		else if (pixformat == PictureFormat::MJpeg)
		{
			// MJPEG
			unsigned char* data = (unsigned char*)bufferMappings[buffer.index].Start;
			size_t dataLength = buffer.bytesused;

			mjpegData = data;
			mjpegDataLength = dataLength;

			//printf("dataLength=%lu\n", dataLength);

			if (isFirstFrame)
			{
				unsigned char* scan = data;
				while (scan < data + dataLength - 4)
				{
					if (scan[0] == MJpegDht[0] &&
						scan[1] == MJpegDht[1] &&
						scan[2] == MJpegDht[2] &&
						scan[3] == MJpegDht[3])
					{
						needsMJpegDht = false;
						break;
					}

					++scan;
				}

				isFirstFrame = false;

				fprintf(stderr, "needsMjpegDht = %d\n", needsMJpegDht);


				// jpeg-turbo
				int api = tjDecompressHeader3(jpegDecompressor,
					data,
					dataLength,
					&jpegWidth,
					&jpegHeight,
					&jpegSubsamp,
					&jpegColorspace);
				if (api != 0)
				{
					char* message = tjGetErrorStr();
					fprintf(stderr, "tjDecompressHeader3 failed (%s)\n", message);
				}
				else
				{
					fprintf(stderr, "jpegWidth=%d jpegHeight=%d jpegSubsamp=%d jpegColorspace=%d\n",
						jpegWidth, jpegHeight, jpegSubsamp, jpegColorspace);
				}

				// TODO: fail if not YUV422

				unsigned long jpegYuvSize = tjBufSizeYUV2(jpegWidth,
					1,
					jpegHeight,
					jpegSubsamp);

				//jpegYuv = new unsigned char[jpegYuvSize];
				jpegYuv = (unsigned char*)yuvSourcePtr;

				fprintf(stderr, "jpegYuv=%p jpegYuvSize=%lu\n",
					jpegYuv, jpegYuvSize);


				frameRate = timer.Interval();
				timer.Start();
			}


#if 0
			ssize_t writeCount = write(fdOut, data, buffer.bytesused);
			if (writeCount < 0)
			{
				throw Exception("write failed.");
			}
#endif


			// jpeg-turbo
			int api = tjDecompressToYUV2(jpegDecompressor,
				data,
				dataLength,
				jpegYuv,
				jpegWidth,
				1,
				jpegHeight,
				TJFLAG_FASTDCT);	//TJFLAG_ACCURATEDCT
			if (api != 0)
			{
				char* message = tjGetErrorStr();
				fprintf(stderr, "tjDecompressToYUV2 failed (%s)\n", message);
			}
			else
			{
				encodeMutex.Lock();


				// Blit - convert YUV422 to NV12

				YuvSource->Sync();


				config_para_s config = { 0 };

				config.src_dst_type = ALLOC_ALLOC; //ALLOC_ALLOC; //ALLOC_OSD0;
				config.alu_const_color = 0xffffffff;

				config.src_format = GE2D_LITTLE_ENDIAN | GE2D_FORMAT_M24_YUV422;
				config.src_planes[0].addr = YuvSource->PhysicalAddress();
				config.src_planes[0].w = width;
				config.src_planes[0].h = height;
				config.src_planes[1].addr = config.src_planes[0].addr + (width * height);
				config.src_planes[1].w = width / 2;
				config.src_planes[1].h = height;
				config.src_planes[2].addr = config.src_planes[1].addr + ((width / 2) * height);
				config.src_planes[2].w = width / 2;
				config.src_planes[2].h = height;

				config.dst_format = GE2D_LITTLE_ENDIAN | GE2D_FORMAT_M24_NV21; //GE2D_FORMAT_M24_NV21; //GE2D_FORMAT_S32_ARGB;
				config.dst_planes[0].addr = YuvDestination->PhysicalAddress();
				config.dst_planes[0].w = width;
				config.dst_planes[0].h = height;
				config.dst_planes[1].addr = config.dst_planes[0].addr + (format.fmt.pix.width * format.fmt.pix.height);
				config.dst_planes[1].w = width;
				config.dst_planes[1].h = height / 2;

				io = ioctl(ge2d_fd, GE2D_CONFIG, &config);
				if (io < 0)
				{
					throw Exception("GE2D_CONFIG failed");
				}

				// Perform the blit operation
				ge2d_para_s blitRect = { 0 };

				blitRect.src1_rect.x = 0;
				blitRect.src1_rect.y = 0;
				blitRect.src1_rect.w = width;
				blitRect.src1_rect.h = height;

				blitRect.dst_rect.x = 0;
				blitRect.dst_rect.y = 0;
				blitRect.dst_rect.w = width;
				blitRect.dst_rect.h = height;

				io = ioctl(ge2d_fd, GE2D_BLIT_NOALPHA, &blitRect);
				if (io < 0)
				{
					throw Exception("GE2D_BLIT_NOALPHA failed.");
				}


				if (timeStamp)
				{
					TimeStamp(width, height);
				}

				YuvDestination->Sync();

				encodeMutex.Unlock();
			}
		}
		else
		{
			throw Exception("Unsupported PictureFromat.");
		}




#if 1
		//Preview
		config_para_s config = { 0 };

		config.src_dst_type = ALLOC_OSD0;
		config.alu_const_color = 0xffffffff;

		config.src_format = GE2D_LITTLE_ENDIAN | GE2D_FORMAT_M24_NV21;
		config.src_planes[0].addr = YuvDestination->PhysicalAddress();
		config.src_planes[0].w = width;
		config.src_planes[0].h = height;
		config.src_planes[1].addr = config.src_planes[0].addr + (width * height);
		config.src_planes[1].w = width;
		config.src_planes[1].h = height / 2;

		//config.src_format = GE2D_LITTLE_ENDIAN | GE2D_FORMAT_S32_ARGB;
		//config.src_planes[0].addr = YuvSource.PhysicalAddress;
		//config.src_planes[0].w = width;
		//config.src_planes[0].h = height;

		config.dst_format = GE2D_FORMAT_S32_ARGB;

		io = ioctl(ge2d_fd, GE2D_CONFIG, &config);
		if (io < 0)
		{
			throw Exception("GE2D_CONFIG failed");
		}

		// Perform the blit operation
		ge2d_para_s blitRect = { 0 };

		blitRect.src1_rect.x = 0;
		blitRect.src1_rect.y = 0;
		blitRect.src1_rect.w = width;
		blitRect.src1_rect.h = height;

		blitRect.dst_rect.x = 0;
		blitRect.dst_rect.y = 0;
		blitRect.dst_rect.w = width;
		blitRect.dst_rect.h = height;

		// Note GE2D_STRETCHBLIT_NOALPHA is required to operate properly
		io = ioctl(ge2d_fd, GE2D_STRETCHBLIT_NOALPHA, &blitRect);
		if (io < 0)
		{
			throw Exception("GE2D_STRETCHBLIT_NOALPHA failed.");
		}

		//printf("GE2D Blit OK.\n");
#endif


		// return buffer
		io = ioctl(captureDev, VIDIOC_QBUF, &buffer);
		if (io < 0)
		{
			throw Exception("VIDIOC_QBUF failed.");
		}


		// Measure FPS
		++frames;
		totalTime += (float)sw.Elapsed(); //GetTime();
		
		sw.Reset();

		if (totalTime >= 1.0f)
		{
			int fps = (int)(frames / totalTime);
			fprintf(stderr, "FPS: %i\n", fps);

			frames = 0;
			totalTime = 0;
		}
	}


	return 0;
}
