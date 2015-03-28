#include <opencv2\opencv.hpp>
#include <Polycode.h>
#include <pxcstatus.h>
#include "Option.h"

namespace mobamas {

struct DepthMap;

std::vector<Polycode::Vector3> ActualVertexPositions(Polycode::SceneMesh *mesh);
void DisplayPinchMats(DepthMap const& depth_map, Option<cv::Point> pinch_point);
void ReportPxcBadStatus(const pxcStatus& status);

}