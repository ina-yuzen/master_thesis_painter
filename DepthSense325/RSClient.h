#pragma once
#include <pxcsession.h>
#include <opencv2\opencv.hpp>
#include <mutex>
#include <memory>
#include <Polycode.h>
#include "DepthMap.h"
#include "PinchTracker.h"

namespace mobamas {

struct Context;

class RSClient :public Polycode::EventHandler
{
public:
	RSClient(std::shared_ptr<Context> context) : context_(context), tracker_(context) {}
	~RSClient();
	bool Prepare();
	void Run();
	void Quit();
	DepthMap last_depth_map() { 
		std::lock_guard<std::mutex> lock(mutex_);
		return last_depth_map_; 
	}
	void handleEvent(Polycode::Event *e);

private:
	volatile bool should_quit_ = false;
	std::shared_ptr<Context> context_;
	PinchTracker tracker_;
	PXCSenseManager *sm_;
	DepthMap last_depth_map_;
	std::mutex mutex_;
	float kX, kY, kYOffset;
	uint16_t kZFar;
};

}
