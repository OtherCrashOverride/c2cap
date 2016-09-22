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



const char* DEFAULT_DEVICE = "/dev/video0";
const char* DEFAULT_OUTPUT = "default.h264";
const int BUFFER_COUNT = 8;

const int DEFAULT_WIDTH = 640;
const int DEFAULT_HEIGHT = 480;
const int DEFAULT_FRAME_RATE = 30;
const int DEFAULT_BITRATE = 1000000 * 5;

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
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
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
		

		// return buffer
		io = ioctl(captureDev, VIDIOC_QBUF, &buffer);
		if (io < 0)
		{
			throw Exception("VIDIOC_QBUF failed.");
		}
	}


	return 0;
}


#if 0
int test_main()
{
	// Load the NV12 test data
	int fd = open("maxresdefault.yuv", O_RDONLY);
	if (fd < 0)
	{
		printf("open failed.\n");
		throw std::exception();
	}

	off_t length = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	std::vector<char> data(length);
	ssize_t cnt = read(fd, &data[0], length);

	close(fd);

	if (cnt != length)
	{
		printf("read failed.\n");
		throw std::exception();
	}


	// Create an output file
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	int fdOut = open("test.h264", O_CREAT| O_TRUNC | O_WRONLY, mode);
	if (fdOut < 0)
	{
		printf("open test.h24 failed\n");
		throw std::exception();
	}


	// Initialize the encoder
	vl_codec_id_t codec_id = CODEC_ID_H264;
	int width = 1280;
	int height = 720;
	int frame_rate = 30;
	int bit_rate = 1000000 * 10;
	int gop = 10;
	vl_img_format_t img_format = IMG_FMT_NV12;

	vl_codec_handle_t handle = vl_video_encoder_init(codec_id, width, height, frame_rate, bit_rate, gop, img_format);
	printf("handle = %ld\n", handle);

	
	// Encode the video frames
	const int BUFFER_SIZE = 1024 * 32;
	char buffer[BUFFER_SIZE];
	for (int i = 0; i < 30 * 30; ++i)
	{
		vl_frame_type_t type = FRAME_TYPE_AUTO;
		char* in = &data[0];
		int in_size = BUFFER_SIZE;
		char* out = buffer;
		int outCnt = vl_video_encoder_encode(handle, type, in, in_size, &out);
		printf("vl_video_encoder_encode = %d\n", outCnt);

		if (outCnt > 0)
		{
			write(fdOut, buffer, outCnt);
		}
	}


	// Close the decoder
	int vl_video_encoder_destory(handle);


	// Close the output file
	close(fdOut);

	return 0;
}

#endif

