#include "HandVisualization.h"

#include "DepthMap.h"
#include "EditorApp.h"
#include "RSClient.h"
#include "Util.h"

namespace mobamas {

HandVisualization::HandVisualization(Polycode::Scene* scene, std::shared_ptr<RSClient> rs_client) :
	scene_(scene), rs_client_(rs_client) 
{
	mesh_ = new Polycode::SceneMesh(Polycode::Mesh::POINT_MESH);
	scene_->addEntity(mesh_);
}

const size_t kSamplingStep = 5;
const float kDepthRatio = 0.02f;

void HandVisualization::Update() {
	auto raw = mesh_->getMesh();
	raw->clearMesh();
	auto depth_map = rs_client_->last_depth_map();
	if (depth_map.raw_mat.empty() || depth_map.binary.empty()) {
		return;
	}
	auto src_width = depth_map.w;
	auto src_height = depth_map.h;
	auto roi = cv::Rect(depth_map.offset, depth_map.offset + cv::Point(src_width, src_height));
	cv::Mat extracted;
	depth_map.raw_mat(roi).copyTo(extracted, depth_map.binary(roi));
	for (int y = 0; y < src_height; y += kSamplingStep)
	{
		for (int x = 0; x < src_width; x += kSamplingStep)
		{
			auto val = extracted.at<ushort>(y, x);
			if (val == 0 || val == depth_map.saturated_value)
				continue;
			auto ray = scene_->projectRayFromCameraAndViewportCoordinate(
				scene_->getActiveCamera(), CameraPointToScreen(x / static_cast<float>(src_width), y / static_cast<float>(src_height)));
			auto point = ray.origin + ray.direction * (val * kDepthRatio);
			raw->addVertex(point.x, point.y, point.z);
		}
	}
}

}
