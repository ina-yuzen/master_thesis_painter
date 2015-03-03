#pragma once
#include <opencv2\opencv.hpp>

namespace mobamas { 
class Recorder {
public:
	Recorder();
	Recorder(const char* video_file);
	~Recorder();		
		void WriteFrame(const cv::Mat& f);
	void SaveSampleImage(const std::string& name, const cv::Mat& m);
	void toggleSampleImage() {
		is_sampling_ = !is_sampling_;
	}
	
private:
	bool is_sampling_;
	CvVideoWriter *video_;
};
}