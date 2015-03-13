#include "Algorithms.h"


#include "Context.h"
#include "DepthMap.h"
#include "DSClient.h"
#include "Recorder.h"

namespace mobamas {

const double kDepthThreshMM = 400;
const double kMinHoleSize = 100.0;
const int kMaskInner = 5;
const int kMaskOuter = 15;
const int kRequiredMaskPixels = 30;

struct MaskSet {
	std::vector<cv::Mat> masks;
	void Add(CvSeq *contour);
};

void MaskSet::Add(CvSeq *contour) {
	static auto elem3x3 = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(3, 3));

	IplImage *circle = cvCreateImage(cvSize(kDepthWidth, kDepthHeight), IPL_DEPTH_8U, 1);
	cvSetZero(circle);
	cvDrawContours(circle, contour, cvScalar(255), cvScalar(255), 0, 1);
	CvMoments mu;
	cvMoments(contour, &mu, false);
	cvFloodFill(circle, cvPoint(static_cast<int>(mu.m10/mu.m00), static_cast<int>(mu.m01/mu.m00)), cvScalar(255));
	cv::Mat maskInner, mask;
	cv::dilate(cv::Mat(circle), maskInner, elem3x3, cv::Point(-1, -1), kMaskInner);
	cv::dilate(cv::Mat(circle), mask, elem3x3, cv::Point(-1, -1), kMaskOuter);
	cvReleaseImage(&circle);

	cv::subtract(mask, maskInner, mask);
	if (cv::countNonZero(mask) > kRequiredMaskPixels)
		masks.push_back(mask);
}

struct ClassifiedRegion {
	CvSeq* contour;
	cv::Point center;
	double area;

	ClassifiedRegion(): contour(nullptr), center(), area(0) {}
	ClassifiedRegion(CvSeq* contour);
};

ClassifiedRegion::ClassifiedRegion(CvSeq* contour) {
	this->contour = contour;
	area = cvContourArea(contour);
	CvMoments mu;
	cvMoments(contour, &mu);
	center = cv::Point(static_cast<int>(mu.m10 / mu.m00), static_cast<int>(mu.m01 / mu.m00));
}

cv::Mat MapColorImage(const cv::Mat& src, const DepthMap& data, const cv::Mat& fill) {
	cv::Mat dest(kDepthHeight, kDepthWidth, CV_8UC3, cv::Scalar(0,0,0));
	auto uvs = data.data.uvMap;
	cv::Vec3b black(0, 0, 0);
	for (int y = 0; y < kDepthHeight; y++)
	{
		for (int x = 0; x < kDepthWidth; x++)
		{
			auto uv = uvs[y * kDepthWidth + x];
			if (uv.u > 0.0 && uv.v > 0.0 && uv.u < 1.0 && uv.v < 1.0 &&
				fill.at<uchar>(y, x) > 0) {
				auto cx = static_cast<int>(uv.u * kColorWidth);
				auto cy = static_cast<int>(uv.v * kColorHeight);
				dest.at<cv::Vec3b>(y, x) = src.at<cv::Vec3b>(cy, cx);
			} else {
				dest.at<cv::Vec3b>(y, x) = black;
			}
		}
	}
	return dest;
}

std::vector<cv::Point> PinchNailColor(std::shared_ptr<Context> context, const DepthMap& data) {
	cv::Mat dest;
	cv::threshold(data.raw_mat, dest, kDepthThreshMM, 0x7fff, cv::THRESH_BINARY);
	cv::Mat fill(data.h, data.w, CV_8UC1);
	for (int i = 0; i < dest.rows; i++){
		for (int j = 0; j < dest.cols; j++){
			auto n = dest.at<int16_t>(i, j);
			if (n == 0x7fff) {
				fill.at<uchar>(i, j) = 0;
			}
			else {
				fill.at<uchar>(i, j) = 0xff;
			}
		}

	}
	IplImage fillIpl = fill;
	cvShowImage("threshold", &fillIpl);

	IplImage *writeTo = cvCreateImage(cvSize(data.w, data.h), IPL_DEPTH_8U, 3);
	MaskSet masks;
	IplImage background = data.normalized;
	std::vector<cv::Point> found;
	cvMerge(&fillIpl, &background ,&background, NULL, writeTo);
	CvMemStorage *storage = cvCreateMemStorage(0);
	CvSeq *cSeq = NULL;
	CvFont font;
	cvInitFont(&font, CV_FONT_HERSHEY_PLAIN, 1, 1);
	cvFindContours(&fillIpl, storage, &cSeq, sizeof(CvContour), CV_RETR_LIST, CV_CHAIN_APPROX_NONE);
	while (cSeq != NULL) {
		if (cSeq->flags & CV_SEQ_FLAG_HOLE && cvContourArea(cSeq) > kMinHoleSize) {
			char buf[128];
			sprintf(buf, "%lf", cvContourArea(cSeq));
			cvPutText(writeTo, buf, ((CvChain*)cSeq)->origin, &font, CV_RGB(0,0,0));
			cvDrawContours(writeTo, cSeq, CV_RGB(255, 0, 0), CV_RGB(0, 0, 255), 0, 3);
			masks.Add(cSeq);
		}
		cSeq = cSeq->h_next;
	}
	cvReleaseMemStorage(&storage);


	if (masks.masks.size() > 0) {
		cv::Mat color = MapColorImage(context->ds_client->LastColorData(), data, fill);
		cvShowImage("color", &((IplImage)context->ds_client->LastColorData()));
		cv::Mat hls;
		cvtColor(color, hls, CV_BGR2HLS);
		cv::Mat debug_out = cv::Mat(kDepthHeight * masks.masks.size(), kDepthWidth, CV_8UC3, cv::Scalar(0, 0, 0));
		int counts = masks.masks.size();
		for (int i = 0; i < counts; i++) {
			cv::Mat this_out(debug_out, cv::Rect(0, i*kDepthHeight, kDepthWidth, kDepthHeight));
			auto mask = masks.masks[i];
			auto maskArea = cv::countNonZero(mask);

			// create kmeans input samples
			cv::Mat samples(maskArea, 2, CV_32F);
			int sampleIdx = 0;
			for (int y = 0; y < kDepthHeight; y++)
			{
				for (int x = 0; x < kDepthWidth; x++)
				{
					if (mask.at<uchar>(y, x) == 0)
						continue;
					auto v = hls.at<cv::Vec3b>(y, x);
					samples.at<float>(sampleIdx, 0) = v[0]; // hue
					samples.at<float>(sampleIdx, 1) = v[2]; // saturation
					sampleIdx++;
				}
			}

			cv::Mat labels;
			cv::kmeans(samples, 2, labels, cv::TermCriteria(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 10000, 0.0001), 5, cv::KMEANS_PP_CENTERS);
			color.copyTo(this_out, mask);

			// map back the results to a binary image
			cv::Mat label1(kDepthHeight, kDepthWidth, CV_8UC1, cv::Scalar(0));
			sampleIdx = 0;
			for (int y = 0; y < kDepthHeight; y++)
			{
				for (int x = 0; x < kDepthWidth; x++)
				{
					if (mask.at<uchar>(y, x) == 0)
						continue;
					if (labels.at<int>(sampleIdx) == 1) {
						label1.at<uchar>(y, x) = 0xff;
					}
					sampleIdx++;
				}
			}
			// swap 0-1 when labelled area is dominant
			if (cv::countNonZero(label1) > maskArea / 2) {
				cv::bitwise_not(label1, label1, mask);
			}
			CvMemStorage *storage = cvCreateMemStorage(0);
			CvSeq *contour = NULL;
			cvFindContours(&(IplImage(label1)), storage, &contour, sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
			ClassifiedRegion dominant;
			while (contour != NULL) {
				ClassifiedRegion cur(contour);
				if (dominant.area < cur.area)
					dominant = cur;
				contour = contour->h_next;
			}
			if (dominant.area != 0 && dominant.area < maskArea / 3) {
				found.push_back(dominant.center);
				cvDrawContours(&((IplImage)this_out), dominant.contour, CV_RGB(0xcc, 0xcc, 0xcc), CV_RGB(0xcc, 0xcc, 0xcc), 0);
				cv::circle(this_out, dominant.center, 4, cv::Scalar(0, 0, 0xff), -1);
				cvCircle(writeTo, dominant.center, 4, CV_RGB(0xff, 0, 0), -1);
			}
			cvReleaseMemStorage(&storage);
		}
		cvShowImage("masked", &((IplImage)debug_out));
	}

	cvShowImage("result", writeTo);
	context->recorder->WriteFrame(cv::Mat(writeTo));
	cvReleaseImage(&writeTo);

	return found;
}

}