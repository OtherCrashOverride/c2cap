all:
	g++ -g -O3 -std=c++11 main.cpp IonBuffer.cpp FontData.cpp -o c2cap -l vpcodec -L/usr/lib/aml_libs/ -lasound -lturbojpeg -lrt

