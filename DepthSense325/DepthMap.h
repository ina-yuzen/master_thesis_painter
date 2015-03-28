#pragma once
#include <stdint.h>
#include <opencv2\opencv.hpp>

namespace mobamas {

struct DepthMap {
	int32_t w, h;
	uint16_t saturated_value;
	cv::Mat raw_mat;
	cv::Mat normalized;
	cv::Mat binary;  // binary image in which white is interested
};

}