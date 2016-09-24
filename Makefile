all:
	g++ -g -O2 -std=c++11 main.cpp -o c2cap -l vpcodec -L/usr/lib/aml_libs/ -lamcodec -lamadec -lamavutils -lasound

