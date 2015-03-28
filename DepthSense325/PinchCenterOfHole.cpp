#include "Algorithms.h"

#include "Context.h"
#include "DepthMap.h"
#include "Recorder.h"
#include "Util.h"

namespace mobamas {

const double kMinHoleSize = 60.0;

static CvSeq* FindLargeHole(CvSeq *parent) {
	auto hole = parent->v_next;
	while (hole) {
		if (cvContourArea(hole) > kMinHoleSize)
			return hole;
		hole = hole->h_next;
	}
	return NULL;
}

Option<cv::Point3f> PinchCenterOfHole(std::shared_ptr<Context> context, const DepthMap& data) {
	IplImage fillIpl = data.binary;
	CvMemStorage *storage = cvCreateMemStorage(0);
	CvSeq *cSeq = NULL;
	cvFindContours(&fillIpl, storage, &cSeq, sizeof(CvContour), CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE);

	Option<cv::Point3f> found = Option<cv::Point3f>::None();
	double max_area = 0;
	while (cSeq != NULL) {
		if (cvContourArea(cSeq) > max_area) {
			found.Clear();
			// hand with pinch hole
			if (CvSeq* hole = FindLargeHole(cSeq)) {

				CvMoments mu;
				cvMoments(hole, &mu, false);
				if (mu.m00 == 0) {
					cSeq = cSeq->h_next;
					continue;
				}
				auto mass_center = cv::Point2d(mu.m10 / mu.m00, mu.m01 / mu.m00);
				found.Reset(cv::Point3f(
					mass_center.x / static_cast<float>(data.w),
					mass_center.y / static_cast<float>(data.h),
					100)); // TOOD: estimate depth from depthmap
			}
		}
		cSeq = cSeq->h_next;
	}
	cvReleaseMemStorage(&storage);
	
	DisplayPinchMats(data, found);

	return found;
}

}