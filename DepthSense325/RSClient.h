#pragma once
#include <pxcsession.h>
#include <opencv2\opencv.hpp>
#include <mutex>
#include <memory>
#include "PinchTracker.h"

namespace mobamas {

struct Context;

class RSClient
{
public:
	RSClient(std::shared_ptr<Context> context) : context_(context), tracker_(context) {}
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
	std::shared_ptr<Context> context_;
	PinchTracker tracker_;
	PXCSenseManager *sm_;
	cv::Mat segmented_depth_;
	std::mutex mutex_;
};

}
