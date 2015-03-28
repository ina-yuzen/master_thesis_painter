#include "Recorder.h"

namespace mobamas {

Recorder::Recorder(): is_sampling_(false), video_(nullptr) {
}

Recorder::Recorder(const char* video_file): is_sampling_(false),
	video_(cvCreateVideoWriter(video_file, CV_FOURCC('P', 'I', 'M', '1'), 24, cvSize(320, 240))) {
		if (!video_) {
			throw std::exception("Failed to open video channel");
		}
}

Recorder::~Recorder() {
	if (video_ != nullptr) {
		cvReleaseVideoWriter(&video_);
	}
}

void Recorder::WriteFrame(const cv::Mat& f) {
	if (video_)
		cvWriteFrame(video_, &((IplImage)f));
}

void Recorder::SaveSampleImage(const std::string& name, const cv::Mat& m) {
	if (!is_sampling_)
		return;
	IplImage tmp = m;
	cvSaveImage(name.c_str(), &tmp);
}

}