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
		prev_(Option<cv::Point3f>::None()) {}
	void NotifyNewData(DepthMap const& depth_map, Option<cv::Point3f> const& data);

private:
	std::shared_ptr<Context> context_;
	Option<cv::Point3f> prev_;
};

}
