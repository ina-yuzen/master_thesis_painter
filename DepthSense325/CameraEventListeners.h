#pragma once
#include <opencv2\opencv.hpp>

namespace mobamas {

class PinchEventListener {
public:
	void OnPinchStart(cv::Point3f point);
	void OnPinchMove(cv::Point3f point);
	void OnPinchEnd(cv::Point3f point);
};

}