#include "Algorithms.h"

#include "Context.h"
#include "DepthMap.h"
#include "Recorder.h"
#include "Util.h"

namespace mobamas {

const double kMinHoleSize = 60.0;

static bool HasHandHole(CvSeq *parent) {
	auto hole = parent->v_next;
	while (hole) {
		if (cvContourArea(hole) > kMinHoleSize)
			return true;
		hole = hole->h_next;
	}
	return false;
}

static CvSeq* FindLargeHole(CvSeq *parent) {
	auto hole = parent->v_next;
	while (hole) {
		if (cvContourArea(hole) > kMinHoleSize)
			return hole;
		hole = hole->h_next;
	}
	return NULL;
}

Option<cv::Point3f> PinchRightEdge(std::shared_ptr<Context> context, const DepthMap& data) {
	assert(!data.raw_mat.empty());
	assert(!data.binary.empty());
	CvMemStorage *storage = cvCreateMemStorage(0);
	CvSeq *cSeq = NULL;
	cvFindContours(&((IplImage)data.binary), storage, &cSeq, sizeof(CvContour), CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE);

	Option<cv::Point3f> found = Option<cv::Point3f>::None();
	double max_area = 0;
	while (cSeq != NULL) {
		if (cvContourArea(cSeq) > max_area) {
			found.Clear();
			// hand with pinch hole
			if (CvSeq* hole = FindLargeHole(cSeq)) {
				CvSeqReader reader;
				cvStartReadSeq(hole, &reader, 0);
				double max_y = 0;
				double min_y = DBL_MAX;
				for (int i = 0; i < hole->total; i++) {
					cv::Point pt;
					CV_READ_SEQ_ELEM(pt, reader);
					if (pt.y > max_y) max_y = pt.y;
					if (pt.y < min_y) min_y = pt.y;
				}
				cvStartReadSeq(cSeq, &reader, 0);
				cv::Point3f right_most_point(0,0,0);
				for (int i = 0; i < cSeq->total; i++) {
					cv::Point pt;
					CV_READ_SEQ_ELEM(pt, reader);
					if (right_most_point.x < pt.x && pt.y >= min_y && pt.y <= max_y) {
						right_most_point.x = pt.x;
						right_most_point.y = pt.y;
						right_most_point.z = data.raw_mat.at<uint16_t>(pt.y, pt.x);
					}
				}
				found.Reset(cv::Point3f(
					right_most_point.x / static_cast<float>(data.w),
					right_most_point.y / static_cast<float>(data.h),
					right_most_point.z)); // TOOD: estimate depth from depthmap
			}
		}
		cSeq = cSeq->h_next;
	}
	cvReleaseMemStorage(&storage);
	
	DisplayPinchMats(data, found);

	return found;
}

}