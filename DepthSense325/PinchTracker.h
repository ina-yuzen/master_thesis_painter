#pragma once

#include <opencv2\opencv.hpp>
#include <memory>
#include "Option.h"

namespace mobamas {

struct Context;
struct DepthMap;

class PinchTracker {
public:
	explicit PinchTracker(std::shared_ptr<Context> context) :
		context_(context), 
		prev_(std::deque<Option<cv::Point3f>>()),
		pinching_(false) {}
	void NotifyNewData(Option<cv::Point3f> const& data);

private:
	std::shared_ptr<Context> context_;
	std::deque<Option<cv::Point3f>> prev_;
	bool pinching_;
	Option<cv::Point3f> Pop();
	void Push(Option<cv::Point3f> point);
	Option<cv::Point3f> PopDense();
};

}
