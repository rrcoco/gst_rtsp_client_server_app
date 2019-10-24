// ConsoleApplication1.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <thread>
#include "../GstServer/Server.h"
#include <fstream>
#include <string>
#include <iterator>
#include <vector>
#include <opencv2/opencv.hpp>
#include <atomic>

using namespace std;

static constexpr auto confFile = "rtsp_parameters.conf";

void feed_frame() {
	static auto count = 0;

	auto name = "frame" + std::to_string(count++) + ".jpg";

	cv::Mat image = cv::imread("C:\\Users\\Hakan\\Downloads\\output\\" + name);
	/*cv::cvtColor(image, image, cv::COLOR_BGR2RGB);*/

	size_t sizeInBytes = image.total() * image.elemSize();

	FeedData(image.data, sizeInBytes);

	count %= 700;
}

int main(int argc,char** argv) {

	Init(feed_frame, confFile);

	RTSPStream(true, false, false);
	
	return 0;
}

