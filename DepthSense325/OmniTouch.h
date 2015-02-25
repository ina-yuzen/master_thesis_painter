#pragma once

#include <opencv2/opencv.hpp>
#include <DepthSense.hxx>

cv::Vec3i detectTouch(const DepthSense::DepthNode::NewSampleReceivedData& data);