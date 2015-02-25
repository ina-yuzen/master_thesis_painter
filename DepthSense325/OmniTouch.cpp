#include "OmniTouch.h"

const int kWinSize = 5;
const int kSmoothThres = 0x150;

static void debugImage(std::string name, cv::Mat& m) {
	IplImage toShow = m;
	cvShowImage(name.c_str(), &toShow);
	cv::waitKey(1);
}

cv::Range estimateFingerWidthPixel(const DepthSense::DepthNode::NewSampleReceivedData& data, int nx, int ny) {
	int32_t w, h;
    FrameFormat_toResolution(data.captureConfiguration.frameFormat,&w,&h);
	const DepthSense::Vertex* vertices = data.vertices;
	float dy; // mm / px
	if (ny == 0)
		dy = vertices[(ny+1) * w + nx].y - vertices[ny * w + nx].y;
	else if (ny < h - 1)
		dy = (vertices[(ny+1) * w + nx].y - vertices[(ny-1) * w + nx].y) / 2.0;
	else
		dy = vertices[ny * w + nx].y - vertices[(ny-1) * w + nx].y;

	return cv::Range((int)(5.0 / dy), (int)(25.0 / dy) + 1);
}

enum Tendency {
	Pos, Neg, Smooth
};

Tendency classifyTendency(const cv::Mat& range) {
	int npos = 0, nneg = 0, nsmooth = 0;
	auto end = range.end<__int16>();
	for (auto it = range.begin<__int16>(); it != end; it++) {
		int v = *it;
		if (v < -kSmoothThres) nneg ++;
		else if (v > kSmoothThres) npos ++;
		else nsmooth ++;
	}
	if (npos > nneg && npos > nsmooth)
		return Pos;
	if (nneg > npos && nneg > nsmooth)
		return Neg;
	return Smooth;
}

cv::Vec3i detectTouch(const DepthSense::DepthNode::NewSampleReceivedData& data) {
	int32_t w, h;
    FrameFormat_toResolution(data.captureConfiguration.frameFormat,&w,&h);
	auto dp = (const int16_t *)data.depthMap;
	cv::Mat depth_image(h, w, CV_16SC1, (void*)dp);

	auto ddepth = -1;
	cv::Mat grad_x;
	cv::Sobel(depth_image, grad_x, ddepth, 1, 0, kWinSize, 1, 0, cv::BORDER_DEFAULT);
	cv::Mat grad_y;
	cv::Sobel(depth_image, grad_y, ddepth, 0, 1, kWinSize, 1, 0, cv::BORDER_DEFAULT);
	debugImage("y", grad_y);

	cv::Mat finger_regions(grad_y.rows, grad_y.cols, CV_8UC3, cv::Scalar(0, 0, 0));
	for (int nx = 0; nx < grad_y.cols; nx++)
	{
		int state = 0; // 0: init, 1: pos found, 2: smooth found, 3: neg found
		int count = 0; // continuous tendency count
		int seq_start_y = 0;
		Tendency currentTend = Smooth;
	    for (int ny = 0; ny < grad_y.rows - 3; ny++)
		{
			auto tend = classifyTendency(grad_y(cv::Rect(nx, ny, 1, 3)));
			if (currentTend == tend) {
				count ++;
			} else {
				currentTend = tend;
				count = 0;
			}
			switch (state) {
			case 0:
				seq_start_y = ny - 3;
				if (currentTend == Neg && count > 2)
					state = 1;
				break;
			case 1:
				if (currentTend == Pos)
					state = 0;
				else if (currentTend == Smooth && count > 5)
					state = 2;
				break;
			case 2:
				if (currentTend == Neg)
					state = 0;
				else if (currentTend == Pos && count > 2) {
					state = 0;
					auto range = estimateFingerWidthPixel(data, nx, (ny + seq_start_y) / 2);
					auto dy = ny - seq_start_y;
					std::cout << nx << ", " << ny << ": " << range.start << " - " << range.end << " : " << dy << std::endl;
					if (dy > range.start && dy < range.end)
						finger_regions(cv::Rect(nx, seq_start_y, 1, dy + 1)) = cv::Scalar(0,255,0);
					else
						finger_regions(cv::Rect(nx, seq_start_y, 1, dy + 1)) = cv::Scalar(0,100,0);
				}
				break;
			}
			int diff = grad_y.at<__int16>(ny, nx);
			finger_regions.ptr<uchar>(ny)[nx * 3] = diff / 256 + 128;
			finger_regions.ptr<uchar>(ny)[nx * 3+1] = diff / 256 + 128;
			finger_regions.ptr<uchar>(ny)[nx * 3+2] = diff / 256 + 128;
		}
	}
	debugImage("regions", finger_regions);

	return cv::Vec3i();
}