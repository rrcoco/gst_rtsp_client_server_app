// ConsoleApplication1.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "../GstClient/Client.h"
#include <opencv2/opencv.hpp>

constexpr static auto confFile = "rtsp_parameters.conf";

void on_start() {
	printf("client started\n");
}

void on_stop() {
	printf("client stopped\n");
}

/* frame receive testing on c++ */
void on_frame(unsigned char* data, int width, int height, int bpp) {
	static auto count = 0;
	auto file = "C:\\Users\\Hakan\\Downloads\\temp\\frame" + std::to_string(count++) + ".jpg";

	cv::Mat frame(cv::Size(width, height), CV_8UC3, data, cv::Mat::AUTO_STEP);
	cv::imwrite(file, frame);
}

int main()
{
	Init(on_start, on_stop, on_frame, confFile);

	RTSPStream(true, false, false);

	return 0;
}

