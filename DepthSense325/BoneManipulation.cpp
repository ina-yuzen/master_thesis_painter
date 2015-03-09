#include "BoneManipulation.h"

#include <functional>
#include <string>
#include <vector>

#include "Util.h"

namespace mobamas {

const int kMovableBonesDepth = 5;	
const std::vector<std::string> kManipulatableBones = ([] {
	std::vector<std::string> v;
	v.push_back("Bone.001_R.001");
	v.push_back("Bone.001_L.001");
	return v;
})();

Polycode::ScenePrimitive* CreateHandleMarker() {
	auto marker = new Polycode::ScenePrimitive(Polycode::ScenePrimitive::TYPE_VPLANE, 0.2, 0.2);
	marker->setColor(0.8, 0.5, 0.5, 0.8);
	marker->depthTest = false;
	marker->billboardMode = true;
	return marker;
}

BoneManipulation::BoneManipulation(Polycode::Scene *scene, Polycode::SceneMesh *mesh) :
	EventHandler(),
	scene_(scene),
	mesh_(mesh),
	handles_(),
	current_target_(nullptr)
{
	auto skeleton = mesh->getSkeleton();
	std::map<Polycode::Bone*, unsigned int> bone_id_map;
	for (unsigned int bidx = 0; bidx < skeleton->getNumBones(); bidx++)
	{
		auto b = skeleton->getBone(bidx);
		bone_id_map[b] = bidx;
		b->disableAnimation = true;
	}
	for (auto name: kManipulatableBones) {
		BoneHandle handle;
		handle.bone = skeleton->getBoneByName(name);
		handle.marker = CreateHandleMarker();
		handle.bone_id = bone_id_map[handle.bone];
		handles_.push_back(handle);
		scene->addEntity(handle.marker);
	}

	auto input = Polycode::CoreServices::getInstance()->getInput();
	using Polycode::InputEvent;
	input->addEventListener(this, InputEvent::EVENT_MOUSEMOVE);
	input->addEventListener(this, InputEvent::EVENT_MOUSEDOWN);
	input->addEventListener(this, InputEvent::EVENT_MOUSEUP);
}

static bool IsMarkerIntersect(const Polycode::Ray& ray, const BoneHandle& handle) {
	auto center = handle.marker->getPosition();
	auto point = ray.planeIntersectPoint(ray.direction, center);
	return point.distance(center) < 0.1;
}

const int kMouseMiddleButtonCode = 2;
const double kSensitivity = 1;

void BoneManipulation::handleEvent(Polycode::Event *e) {
	using Polycode::InputEvent;

	auto set_click_state = [&](bool select) {
		InputEvent *ie = (InputEvent*)e;
		if (ie->mouseButton != kMouseMiddleButtonCode) 
			return;
		mouse_prev_ = ie->getMousePosition();
		if (select) {
			auto ray = scene_->projectRayFromCameraAndViewportCoordinate(scene_->getActiveCamera(), ie->getMousePosition());
			for (auto &handle: handles_) {
				if (IsMarkerIntersect(ray, handle)) {
					current_target_ = &handle;
					return;
				}
			}
		} else {
			current_target_ = nullptr;
		}
	};

	switch (e->getEventCode()) {
	case InputEvent::EVENT_MOUSEMOVE:
		if (current_target_ != nullptr) {
			auto new_pos = ((InputEvent*)e)->mousePosition;
			auto diff = new_pos - mouse_prev_;
			current_target_->bone->Yaw(diff.x * kSensitivity);
			current_target_->bone->Pitch(diff.y * kSensitivity);
			current_target_->bone->rebuildFinalMatrix();
			mouse_prev_ = new_pos;
		}
		break;
	case InputEvent::EVENT_MOUSEDOWN:
		set_click_state(true);
		break;
	case InputEvent::EVENT_MOUSEUP:
		set_click_state(false);
		break;
	}
}

std::vector<Polycode::Vector3> CalculateBoneCenters(Polycode::SceneMesh* mesh) {
	auto skeleton = mesh->getSkeleton();
	auto raw = mesh->getMesh();
	std::vector<Polycode::Vector3> adjusted_points = ActualVertexPositions(mesh);
	std::vector<Polycode::Vector3> bone_centers(skeleton->getNumBones(), Polycode::Vector3(0, 0, 0));
	std::vector<double> bone_weights(skeleton->getNumBones(), 0);
	auto indices = raw->vertexBoneIndexArray;
	auto weights = raw->vertexBoneWeightArray;
	for (int vi = 0; vi < indices.getDataSize(); vi++) {
		int bi = indices.data[vi];
		double w = weights.data[vi];
		if (w > 0.0) {
			bone_centers[bi] = bone_centers[bi] + adjusted_points[vi / 4] * w;
			bone_weights[bi] += w;
		}
	}
	for (int i = 0; i < bone_centers.size(); i++) {
		if (bone_weights[i] > 0.0)
			bone_centers[i] = bone_centers[i] / bone_weights[i];
	}
	return bone_centers;
}

void BoneManipulation::Update() {
	auto bone_centers = CalculateBoneCenters(mesh_);
	for (auto handle: handles_) {
		handle.marker->setPosition(bone_centers[handle.bone_id]);
	}
}

}