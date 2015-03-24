#pragma once
#include <pxcsession.h>
#include <opencv2\opencv.hpp>

namespace mobamas {

class RSClient
{
public:
	~RSClient();
	bool Prepare();
	void Run();
	void Quit();
	cv::Mat segmented_depth() { return segmented_depth_; }

private:
	volatile bool should_quit_ = false;
	PXCSenseManager *sm_;
	cv::Mat segmented_depth_;
};

}
