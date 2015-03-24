#include "HandVisualization.h"

#include "EditorApp.h"
#include "RSClient.h"

namespace mobamas {

HandVisualization::HandVisualization(Polycode::Scene* scene, std::shared_ptr<RSClient> rs_client) :
	scene_(scene), rs_client_(rs_client) 
{
	mesh_ = new Polycode::SceneMesh(Polycode::Mesh::POINT_MESH);
	scene_->addEntity(mesh_);
}

const size_t kSamplingStep = 5;
const float kDepthRatio = 1.0f;

void HandVisualization::Update() {
	auto raw = mesh_->getMesh();
	raw->clearMesh();
	auto depth = rs_client_->segmented_depth();
	if (depth.empty()) {
		return;
	}
	auto src_width = depth.cols;
	auto src_height = depth.rows;
	float x_ratio = kWinWidth / static_cast<float>(src_width);
	float y_ratio = kWinHeight / static_cast<float>(src_height);
	for (int y = 0; y < src_height; y += kSamplingStep)
	{
		for (int x = 0; x < src_width; x += kSamplingStep)
		{
			auto val = depth.at<ushort>(y, x);
			if (val == 0)
				continue;
			auto ray = scene_->projectRayFromCameraAndViewportCoordinate(
				scene_->getActiveCamera(), Polycode::Vector2(x * x_ratio, y * y_ratio));
			auto point = ray.origin + ray.direction * (val * kDepthRatio);
			raw->addVertex(point.x, point.y, point.z);
		}
	}
}

}
