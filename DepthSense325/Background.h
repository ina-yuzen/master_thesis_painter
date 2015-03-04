#pragma once
#include <opencv2\opencv.hpp>

namespace mobamas{

struct DepthMap;

class Background {
public:
	Background(): given_calibration_frames_(0) {}
	bool Calibrate(const cv::Mat& raw_depth);
	cv::Mat EliminateBackground(const cv::Mat& raw_depth) const;
	void Restart();
	
private:
	int given_calibration_frames_;
	cv::Mat background_depth_;
};

}