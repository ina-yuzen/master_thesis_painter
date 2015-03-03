#include "PinchWilson.h"

#include "Context.h"
#include "DepthMap.h"
#include "Recorder.h"

const double kDepthThreshMM = 400;
const double kMinHoleSize = 100.0;
const int kErosionSize = 3;

namespace mobamas {

std::vector<cv::Point> DetectPinchPoint(std::shared_ptr<Context> context, const DepthMap& data) {
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
	std::vector<cv::Mat> contours;
	std::vector<cv::Vec4i> hierarchy;
	IplImage fillIpl = fill;
	cvShowImage("threshold", &fillIpl);

	cv::Mat dilated = fill.clone();
	auto element = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(kErosionSize * 2 + 1, kErosionSize * 2 + 1), cv::Point(kErosionSize, kErosionSize));
	dilate(fill, dilated, element);

	IplImage *writeTo = cvCreateImage(cvSize(data.w, data.h), IPL_DEPTH_8U, 3);
	IplImage background = data.normalized;
	std::vector<cv::Point> found;
	cvMerge(&fillIpl, &background ,&background, NULL, writeTo);
	CvMemStorage *storage = cvCreateMemStorage(0);
	CvSeq *cSeq = NULL;
	CvFont font;
	cvInitFont(&font, CV_FONT_HERSHEY_PLAIN, 1, 1);
	cvFindContours(&(IplImage)dilated, storage, &cSeq, sizeof(CvContour), CV_RETR_LIST, CV_CHAIN_APPROX_NONE);
	while (cSeq != NULL) {
		
		if (cSeq->total > 10 && cSeq->flags & CV_SEQ_FLAG_HOLE && cvContourArea(cSeq) > kMinHoleSize) {
			char buf[128];
			sprintf(buf, "%lf", cvContourArea(cSeq));
			cvPutText(writeTo, buf, ((CvChain*)cSeq)->origin, &font, CV_RGB(0,0,0));
			// cvDrawContours(writeTo, cSeq, CV_RGB(255, 0, 0), CV_RGB(0, 0, 255), 0, 3);
			
			CvSeqReader reader;
			cvStartReadSeq(cSeq, &reader, 0);
			std::vector<cv::Point> pts;
			for (int i = 0; i < cSeq->total; i++)
			{
				cv::Point pt;
				CV_READ_SEQ_ELEM(pt, reader);
				pts.push_back(pt);
			}
			CvMoments mu;
			cvMoments(cSeq, &mu, false);
			if (mu.m00 == 0) {
				cSeq = cSeq->h_next;
				continue;
			}
			auto massCenter = cv::Point2d(mu.m10/mu.m00, mu.m01/mu.m00);
			for (int i = 0; i < pts.size(); i++)
			{
				auto pt = pts[i];
				cv::Vec2d orientation(pt.x - massCenter.x, pt.y - massCenter.y);
				cv::Vec2d diff = orientation * (float)(kErosionSize) * 1.8 / norm(orientation);
				auto estimated = cv::Point(pt.x + static_cast<int>(diff.val[0]), pt.y + static_cast<int>(diff.val[1]));
				if (estimated.x >= 0 &&
					estimated.y >= 0 &&
					estimated.x < fill.cols &&
					estimated.y < fill.rows &&
					fill.at<uchar>(estimated) == 0) {
						found.push_back(estimated);
						cvCircle(writeTo, estimated, 3, CV_RGB(0,255,255), -1);
				}
			}
		}
		cSeq = cSeq->h_next;
	}
	cvReleaseMemStorage(&storage);
	cvShowImage("result", writeTo);
	context->recorder->WriteFrame(cv::Mat(writeTo));
	cvReleaseImage(&writeTo);

	return found;
}

}