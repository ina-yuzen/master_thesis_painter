#include "BoneManipulation.h"

#include <functional>
#include <string>
#include <vector>

#include "EditorApp.h"
#include "Util.h"

namespace mobamas {

const int kMovableBonesDepth = 5;	
const std::vector<std::string> kManipulatableBones = ([] {
	std::vector<std::string> v;
	v.push_back("Bone.001_R.001");
	v.push_back("Bone.001_L.001");
	v.push_back("Bone_L.002");
	v.push_back("Bone_R.002");
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
			current_target_ = SelectHandleByWindowCoord(ie->getMousePosition());
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

void BoneManipulation::OnPinchStart(cv::Point3f point) {
	pinch_prev_ = point;
	Polycode::Vector2 on_window(kWinWidth * point.x, kWinHeight * point.y);
	current_target_ = SelectHandleByWindowCoord(on_window, 1.0);
	if (current_target_) {
		current_target_->marker->setColor(1.0, 0.5, 0.5, 0.8);
	}
}

static Polycode::Scene *debug_scene = nullptr;
static Polycode::ScreenEntity* debug_point = nullptr;
void BoneManipulation::OnPinchMove(cv::Point3f point) {
	if (debug_point == nullptr) {
		debug_scene = new Polycode::Scene(Polycode::Scene::SCENE_2D_TOPLEFT);
		debug_point = new Polycode::ScenePrimitive(Polycode::ScenePrimitive::TYPE_CIRCLE, 3, 3, 10);
		debug_scene->addEntity(debug_point);
	}
	debug_point->setPositionX(kWinWidth * point.x);
	debug_point->setPositionY(kWinHeight * point.y);
	if (current_target_ == nullptr)
		return;

	auto diff_xy = Polycode::Vector2(point.x - pinch_prev_.x, point.y - pinch_prev_.y).length();
	auto diff_z = point.z - pinch_prev_.z;
	current_target_->bone->Yaw(diff_z * 1);
	current_target_->bone->Pitch(diff_xy * 1);
	current_target_->bone->rebuildFinalMatrix();
	pinch_prev_ = point;
}

void BoneManipulation::OnPinchEnd() {
	if (current_target_) {
		current_target_->marker->setColor(0.8, 0.5, 0.5, 0.8);
		current_target_ = nullptr;
	}
}

BoneHandle* BoneManipulation::SelectHandleByWindowCoord(Polycode::Vector2 point, double allowed_error) {
	auto ray = scene_->projectRayFromCameraAndViewportCoordinate(scene_->getActiveCamera(), point);
	double min_error = 1e10;
	BoneHandle* selected = nullptr;
	for (auto &handle : handles_) {
		auto center = handle.marker->getPosition();
		auto point = ray.planeIntersectPoint(ray.direction, center);
		auto distance = point.distance(center);
		if (distance < std::min(allowed_error, min_error)) {
			min_error = distance;
			selected = &handle;
		}
	}
	return selected;
}


}