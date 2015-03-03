#pragma once

#include <memory>
#include <vector>
#include <opencv2\opencv.hpp>

namespace mobamas {

struct Context;
struct DepthMap;
std::vector<cv::Point> DetectPinchPoint(std::shared_ptr<Context> context, const DepthMap& data);

}