// g++ -g main.cpp -o c2cap -l vpcodec
// ffmpeg -framerate 60 -i test.h264 -vcodec copy test.mp4
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

// The headers are not aware C++ exists
extern "C"
{
	//#include <amcodec/codec.h>
#include <codec.h>
}
// Codec parameter flags
//    size_t is used to make it 
//    64bit safe for use on Odroid C2
const size_t EXTERNAL_PTS = 0x01;
const size_t SYNC_OUTSIDE = 0x02;
const size_t USE_IDR_FRAMERATE = 0x04;
const size_t UCODE_IP_ONLY_PARAM = 0x08;
const size_t MAX_REFER_BUF = 0x10;
const size_t ERROR_RECOVERY_MODE_IN = 0x20;


const char* DEFAULT_DEVICE = "/dev/video0";
const char* DEFAULT_OUTPUT = "default.h264";
const int BUFFER_COUNT = 16;

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
	{ 0, 0, 0, 0 }
};

class Exception : public std::exception
{
public:
	Exception(const char* message)
		: std::exception()
	{
		fprintf(stderr, "%s\n", message);
	}

};


codec_para_t codecContext;
void OpenCodec(int width, int height, int fps)
{
	fps *= 2;

	// Initialize the codec
	codecContext = { 0 };


	codecContext.stream_type = STREAM_TYPE_ES_VIDEO;
	codecContext.video_type = VFORMAT_MJPEG;
	codecContext.has_video = 1;
	codecContext.noblock = 0;
	codecContext.am_sysinfo.format = VIDEO_DEC_FORMAT_MJPEG;
	codecContext.am_sysinfo.width = width;
	codecContext.am_sysinfo.height = height;
	codecContext.am_sysinfo.rate = (96000.0 / fps);	
	codecContext.am_sysinfo.param = (void*)( SYNC_OUTSIDE); //EXTERNAL_PTS |


	int api = codec_init(&codecContext);
	if (api != 0)
	{
		throw Exception("codec_init failed.");		
	}
}

void WriteCodecData(unsigned char* data, int dataLength)
{
	int offset = 0;
	while (offset < dataLength)
	{
		int count = codec_write(&codecContext, data + offset, dataLength - offset);
		if (count > 0)
		{
			offset += count;
		}
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

	int c;
	while ((c = getopt_long(argc, argv, "d:o:w:h:f:b:", longopts, NULL)) != -1)
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

	printf("card = %s\n", (char*)caps.card);
	printf("\tbus_info = %s\n", (char*)caps.bus_info);
	printf("\tdriver = %s\n", (char*)caps.driver);

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

	
	// TODO: format selection from user input / enumeration

	v4l2_format format = { 0 };
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.width = width;
	format.fmt.pix.height = height;
	//format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
	//format.fmt.pix.field = V4L2_FIELD_ANY;

	io = ioctl(captureDev, VIDIOC_S_FMT, &format);
	if (io < 0)
	{
		throw Exception("VIDIOC_S_FMT failed.");
	}

	fprintf(stderr, "v4l2_format: width=%d, height=%d, pixelformat=0x%x\n",
		format.fmt.pix.width, format.fmt.pix.height, format.fmt.pix.pixelformat);


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


	// Create MJPEG codec
	OpenCodec(width, height, fps);


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


#if 0
	// Initialize the encoder
	vl_codec_id_t codec_id = CODEC_ID_H264;
	width = format.fmt.pix.width;
	height = format.fmt.pix.height;
	fps = (int)((double)streamParm.parm.capture.timeperframe.denominator /
				(double)streamParm.parm.capture.timeperframe.numerator);
	//int bit_rate = bitrate;
	int gop = 10;

	fprintf(stderr, "vl_video_encoder_init: width=%d, height=%d, fps=%d, bitrate=%d, gop=%d\n",
		width, height, fps, bitrate, gop);

	vl_img_format_t img_format = IMG_FMT_NV12;
	vl_codec_handle_t handle = vl_video_encoder_init(codec_id, width, height, fps, bitrate, gop, img_format);
	fprintf(stderr, "handle = %ld\n", handle);
#endif


	// Start streaming
	int bufferType = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	io = ioctl(captureDev, VIDIOC_STREAMON, &bufferType);
	if (io < 0)
	{
		throw Exception("VIDIOC_STREAMON failed.");
	}


	int nv12Size = format.fmt.pix.width * format.fmt.pix.height * 4;
	unsigned char nv12[nv12Size];
	fprintf(stderr, "nv12Size = %d\n", nv12Size);

	int ENCODE_BUFFER_SIZE = nv12Size; //1024 * 32;
	char encodeBuffer[ENCODE_BUFFER_SIZE];
	//fprintf(stderr, "ENCODEC_BUFFER_SIZE = %d\n", ENCODEC_BUFFER_SIZE);

	
	bool isFirstFrame = true;
	bool needsMJpegDht = true;

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

#if 0
		// Process frame
		//printf("Got a buffer: index = %d\n", buffer.index);
		unsigned short* data = (unsigned short*)bufferMappings[buffer.index].Start;

		// convert YUYV to NV12
		int srcStride = format.fmt.pix.width; // *sizeof(short);
		int dstStride = format.fmt.pix.width;
		int dstVUOffset = format.fmt.pix.width * format.fmt.pix.height;

		for (int y = 0; y < format.fmt.pix.height; ++y)
		{
			for (int x = 0; x < format.fmt.pix.width; x += 2)
			{
				int srcIndex = y * srcStride + x;
				//unsigned char l = data[srcIndex];
				unsigned short yu = data[srcIndex];
				unsigned short yv = data[srcIndex + 1];


				int dstIndex = y * dstStride + (x);
				nv12[dstIndex] = yu & 0xff;
				nv12[dstIndex + 1] = yv & 0xff;

				if (y % 2 == 0)
				{
					int dstVUIndex = (y >> 1) * dstStride + (x);
					nv12[dstVUOffset + dstVUIndex] = yv >> 8;
					nv12[dstVUOffset + dstVUIndex + 1] = yu >> 8;
				}
			}
		}

		// Encode the video frames
		vl_frame_type_t type = FRAME_TYPE_AUTO;
		char* in = (char*)&nv12[0];
		int in_size = ENCODE_BUFFER_SIZE;
		char* out = encodeBuffer;
		int outCnt = vl_video_encoder_encode(handle, type, in, in_size, &out);
		//printf("vl_video_encoder_encode = %d\n", outCnt);

		if (outCnt > 0)
		{
			ssize_t writeCount = write(fdOut, encodeBuffer, outCnt);
			if (writeCount < 0)
			{
				throw Exception("write failed.");
			}
		}
#endif
		
		// MJPEG
		unsigned char* data = (unsigned char*)bufferMappings[buffer.index].Start;
		size_t dataLength = buffer.bytesused; //bufferMappings[buffer.index].Length;
		
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
		}

#if 0
		ssize_t writeCount = write(fdOut, data, buffer.bytesused);
		if (writeCount < 0)
		{
			throw Exception("write failed.");
		}
#endif


		if (needsMJpegDht)
		{
			// Find the start of scan (SOS)
			unsigned char* sos = data;
			while (sos < data + dataLength - 1)
			{
				if (sos[0] == 0xff && sos[1] == 0xda)
					break;

				++sos;
			}

			// Send everthing up to SOS
			int headerLength = sos - data;
			WriteCodecData(data, headerLength);

			// Send DHT
			WriteCodecData(MJpegDht, MJpegDhtLength);

			// Send remaining data
			WriteCodecData(sos, dataLength - headerLength);

			//printf("dataLength=%lu, found SOS @ %d\n", dataLength, headerLength);
		}
		else
		{
			WriteCodecData(data, dataLength);
		}


		// return buffer
		io = ioctl(captureDev, VIDIOC_QBUF, &buffer);
		if (io < 0)
		{
			throw Exception("VIDIOC_QBUF failed.");
		}
	}


	return 0;
}
