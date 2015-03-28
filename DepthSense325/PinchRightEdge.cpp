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

Option<cv::Point> PinchRightEdge(std::shared_ptr<Context> context, const DepthMap& data) {
	IplImage fillIpl = data.binary;
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
				for (int i = 0; i < cSeq->total; i++) {
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
	
	DisplayPinchMats(data, found);

	return found;
}

}