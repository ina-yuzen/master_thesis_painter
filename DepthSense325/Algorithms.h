#pragma once

#include <memory>
#include <opencv2\opencv.hpp>
#include "Option.h"

namespace mobamas {

struct Context;
struct DepthMap;
Option<cv::Point> PinchRightEdge(std::shared_ptr<Context> context, const DepthMap& data);
Option<cv::Point> PinchCenterOfHole(std::shared_ptr<Context> context, const DepthMap& data);

}