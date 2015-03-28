#pragma once

#include <memory>
#include <opencv2\opencv.hpp>
#include "Option.h"

namespace mobamas {

struct Context;
struct DepthMap;

// Pinch detection algorithms return a most confident pinch point, if any.
// The x,y are ratio (0.0 - 1.0) in the camera resolution, originated at top-left.
// The z is actual depth value in mm.
Option<cv::Point3f> PinchRightEdge(std::shared_ptr<Context> context, const DepthMap& data);
Option<cv::Point3f> PinchCenterOfHole(std::shared_ptr<Context> context, const DepthMap& data);

}