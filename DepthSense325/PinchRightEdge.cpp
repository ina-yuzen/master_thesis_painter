#include "Algorithms.h"

#include "Context.h"
#include "DepthMap.h"
#include "Recorder.h"

namespace mobamas {

const double kDepthThreshMM = 400;
const double kMinHoleSize = 60.0;
const int kMaskInner = 5;
const int kMaskOuter = 15;
const int kRequiredMaskPixels = 30;

bool HasHandHole(CvSeq *parent) {
	auto hole = parent->v_next;
	while (hole) {
		if (cvContourArea(hole) > kMinHoleSize)
			return true;
		hole = hole->h_next;
	}
	return false;
}

Option<cv::Point> PinchRightEdge(std::shared_ptr<Context> context, const DepthMap& data) {
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
	IplImage background = data.normalized;
	cvMerge(&fillIpl, &background ,&background, NULL, writeTo);
	CvMemStorage *storage = cvCreateMemStorage(0);
	CvSeq *cSeq = NULL;
	cvFindContours(&fillIpl, storage, &cSeq, sizeof(CvContour), CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE);

	Option<cv::Point> found = Option<cv::Point>::None();
	double max_area = 0;
	while (cSeq != NULL) {
		if (cvContourArea(cSeq) > max_area) {
			found.Clear();
			// hand with pinch hole
			if (HasHandHole(cSeq)) {
				CvSeqReader reader;
				cvStartReadSeq(cSeq, &reader, 0);
				cv::Point right_most_point(0,0);
				for (int i = 0; i < cSeq->total; i++)
				{
					cv::Point pt;
					CV_READ_SEQ_ELEM(pt, reader);
					if (right_most_point.x < pt.x)
						right_most_point = pt;
				}
				found.Reset(right_most_point);
			}
		}
		cSeq = cSeq->h_next;
	}
	cvReleaseMemStorage(&storage);
	
	if (found)
		cvCircle(writeTo, *found, 3, CV_RGB(255, 0, 60), -1);
	cvShowImage("result", writeTo);
	cvReleaseImage(&writeTo);

	return found;
}

}