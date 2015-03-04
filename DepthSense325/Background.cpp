#include "Background.h"

#include "DepthMap.h"

namespace mobamas {

static const int kRequiredFrames = 10;
static const int kMaxDistanceMM = 400;
static const int kAcceptableErrorMM = 50;
static const int kNearSize = 10;

// if current value is enough similar or all near values are satuated, acceptable
static bool IsAcceptable(int16_t current, const cv::Mat& reference, int x, int y) {
	bool all_satuated = true;
	for (int dx = -kNearSize; dx <= kNearSize; dx ++) {
		int maxy = kNearSize - abs(dx);
		for (int dy = -maxy; dy <= maxy; dy ++) {
			int ox = x + dx, oy = y + dy;
			if (ox >= 0 && ox < reference.cols && oy >= 0 && oy < reference.rows) {
				auto given = reference.at<int16_t>(oy, ox);
				if (given == kSaturatedDepth) {
					continue;
				}
				all_satuated = false;
				if (abs(current - given) < kAcceptableErrorMM)
					return true;
			}
		}
	}
	return all_satuated;
}

bool Background::Calibrate(const cv::Mat& raw_depth) {
	if (given_calibration_frames_ >= kRequiredFrames)
		return true;

	if (given_calibration_frames_ == 0) {
		background_depth_ = cv::Mat(raw_depth.size(), CV_16SC1, kSaturatedDepth);
	}
	given_calibration_frames_ ++;
	cv::Mat result(raw_depth.size(), CV_8UC1, cv::Scalar(0));
	for (int y = 0; y < background_depth_.rows; y++)
	{
		for (int x = 0; x < background_depth_.cols; x++)
		{
			int16_t& cur = background_depth_.at<int16_t>(y, x);
			auto given = raw_depth.at<int16_t>(y, x);
			if (cur == kSaturatedDepth && given < kMaxDistanceMM) {
				cur = given;
				continue;
			}
			if (!IsAcceptable(cur, raw_depth, x, y)) {
				result.at<uchar>(y, x) = 0xff;
				/* std::cout << "Error is too big at " << x << ", " << y << ": " << cur << " - " << given << ". Restarting calibration." << std::endl;
				Restart();
				return false; */
			}
		}
	}
	cvShowImage("error", &((IplImage)result));

	return given_calibration_frames_ >= kRequiredFrames;
}


cv::Mat Background::EliminateBackground(const cv::Mat& raw_depth) const {
	cvShowImage("bg", &((IplImage)background_depth_));
	cv::Mat eliminated;
	raw_depth.copyTo(eliminated);
	for (int y = 0; y < eliminated.rows; y++)
	{
		for (int x = 0; x < eliminated.cols; x++)
		{
			int16_t& cur = eliminated.at<int16_t>(y, x);
			int16_t ref = background_depth_.at<int16_t>(y, x);
			if (cur != kSaturatedDepth && (abs(ref - cur) < kAcceptableErrorMM || cur > kMaxDistanceMM)) {
				cur = kSaturatedDepth;
			}
		}
	}
	return eliminated;
}

void Background::Restart() {
	given_calibration_frames_ = 0;
}

}