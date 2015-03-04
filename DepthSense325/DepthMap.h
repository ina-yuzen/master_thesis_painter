#pragma once
#include <memory>

#include <opencv2\opencv.hpp>
#include <DepthSense.hxx>

namespace mobamas {

const int kSaturatedDepth = 32001;

struct Context;

struct DepthMap {
	int32_t w, h;
	DepthSense::DepthNode::NewSampleReceivedData data;
	cv::Mat raw_mat;
	cv::Mat foreground;
	cv::Mat normalized;
};

void onNewDepthSample(DepthSense::DepthNode node, DepthSense::DepthNode::NewSampleReceivedData data, std::shared_ptr<Context> context);

}