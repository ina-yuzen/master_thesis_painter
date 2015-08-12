#include <opencv2\opencv.hpp>
#include <Polycode.h>
#include <pxcstatus.h>
#include "Option.h"

namespace mobamas {

struct DepthMap;

std::vector<Polycode::Vector3> ActualVertexPositions(Polycode::SceneMesh *mesh);
void DisplayPinchMats(DepthMap const& depth_map, Option<cv::Point3f> const& pinch_point);
void ReportPxcBadStatus(const pxcStatus& status);
Polycode::Vector2 CameraPointToScreen(Number x, Number y);

// Polycode OpenCV conversions
cv::Point ToCv(Polycode::Vector2 const& p, float ratio = 1);
cv::Scalar ToCv(Polycode::Color const& c);

// debug printers
std::ostream& operator<<(std::ostream& os, Polycode::Matrix4 const& mat);
std::ostream& operator<<(std::ostream& os, Polycode::Vector2 const& vec);
std::ostream& operator<<(std::ostream& os, Polycode::Vector3 const& vec);
std::ostream& operator<<(std::ostream& os, Polycode::Quaternion const& quat);

}