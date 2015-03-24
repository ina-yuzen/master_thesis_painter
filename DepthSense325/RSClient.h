#pragma once
#include <pxcsession.h>
#include <opencv2\opencv.hpp>
#include <mutex>

namespace mobamas {

class RSClient
{
public:
	~RSClient();
	bool Prepare();
	void Run();
	void Quit();
	cv::Mat segmented_depth() { 
		std::lock_guard<std::mutex> lock(mutex_);
		return segmented_depth_.clone(); 
	}

private:
	volatile bool should_quit_ = false;
	PXCSenseManager *sm_;
	cv::Mat segmented_depth_;
	std::mutex mutex_;
};

}
