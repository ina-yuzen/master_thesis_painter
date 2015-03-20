#pragma once
#include <opencv2\opencv.hpp>

namespace mobamas {

class PinchEventListener {
public:
	virtual ~PinchEventListener() {}

	virtual void OnPinchStart(cv::Point3f point) = 0;
	virtual void OnPinchMove(cv::Point3f point) = 0;
	virtual void OnPinchEnd() = 0;
};

}