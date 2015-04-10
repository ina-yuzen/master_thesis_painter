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
	pinch_start_sound_ = new Polycode::Sound("Resources/click7.wav");
	pinch_end_sound_ = new Polycode::Sound("Resources/click21.wav");
}

const int kMouseMiddleButtonCode = 0;
const double kSensitivity = 1;

void BoneManipulation::handleEvent(Polycode::Event *e) {
	using Polycode::InputEvent;

	auto fake_3d_coord = [](InputEvent* e) {
		auto point = e->getMousePosition();
		return cv::Point3f(point.x / static_cast<float>(kWinWidth),
			point.y / static_cast<float>(kWinHeight),
			100);
	};

	switch (e->getEventCode()) {
	case InputEvent::EVENT_MOUSEMOVE:
	{
		auto ie = (InputEvent*)e;
		OnPinchMove(fake_3d_coord(ie));
		break;
	}
	case InputEvent::EVENT_MOUSEDOWN:
	{
		auto ie = (InputEvent*)e;
		if (ie->getMouseButton() == kMouseMiddleButtonCode)
			OnPinchStart(fake_3d_coord(ie));
		break;
	}
	case InputEvent::EVENT_MOUSEUP:
	{
		auto ie = (InputEvent*)e;
		if (ie->getMouseButton() == kMouseMiddleButtonCode)
			OnPinchEnd();
		break;
	}
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

static Polycode::Vector2 EstimateXyRotationCenter(Polycode::Scene* scene, std::vector<Polycode::Vector3> const& centers, BoneHandle const* current_target) {
	assert(current_target->bone->parentBoneId >= 0);

	// Prepare rendering environment
	auto renderer = Polycode::CoreServices::getInstance()->getRenderer();
	renderer->BeginRender();
	renderer->setPerspectiveDefaults();
	scene->getActiveCamera()->doCameraTransform();

	auto camera = renderer->getCameraMatrix();
	auto projection = renderer->getProjectionMatrix();
	auto view = renderer->getViewport();
	auto this_pos = renderer->Project(camera, projection, view, centers[current_target->bone_id]);
	auto parent_pos = renderer->Project(camera, projection, view, centers[current_target->bone->parentBoneId]);

	renderer->EndRender();

	return (parent_pos + this_pos) * 0.5;
}
void BoneManipulation::Update() {
	auto bone_centers = CalculateBoneCenters(mesh_);
	for (auto handle: handles_) {
		handle.marker->setPosition(bone_centers[handle.bone_id]);
	}
	if (require_xy_rotation_center_recalculation_ && current_target_) {
		current_target_->marker->setColor(1.0, 0.5, 0.5, 0.8);
		xy_rotation_center_ = EstimateXyRotationCenter(scene_, CalculateBoneCenters(mesh_), current_target_);
		require_xy_rotation_center_recalculation_ = false;
	}
}

static Polycode::Vector2 PinchPointOnWindow(cv::Point3f const& point) {
	return Polycode::Vector2(kWinWidth * point.x, kWinHeight * point.y);
}
void BoneManipulation::OnPinchStart(cv::Point3f point) {
	pinch_prev_ = point;
	current_target_ = SelectHandleByWindowCoord(PinchPointOnWindow(point), 1.0);
	require_xy_rotation_center_recalculation_ = true; // flag for calculating after that
	if (current_target_ != nullptr)
		pinch_start_sound_->Play();
}

static Polycode::Scene *debug_scene = nullptr;
static Polycode::ScreenEntity* debug_point = nullptr;
static void DisplayDebugPoint(cv::Point3f const& point) {
	if (debug_point == nullptr) {
		debug_scene = new Polycode::Scene(Polycode::Scene::SCENE_2D_TOPLEFT);
		debug_point = new Polycode::ScenePrimitive(Polycode::ScenePrimitive::TYPE_CIRCLE, 3, 3, 10);
		debug_scene->addEntity(debug_point);
	}
	debug_point->setPositionX(kWinWidth * point.x);
	debug_point->setPositionY(kWinHeight * point.y);
}

// http://lolengine.net/blog/2013/09/18/beautiful-maths-quaternion-from-vectors
static Polycode::Quaternion FromTwoVectors(Polycode::Vector3 const& u, Polycode::Vector3 const& v) {
	auto w = u.crossProduct(v);
	Polycode::Quaternion q(u.dot(v), w.x, w.y, w.z);
	q.w += sqrt(q.Norm());
	q.Normalize();
	return q;
}

void BoneManipulation::OnPinchMove(cv::Point3f point) {
#ifdef _DEBUG
	DisplayDebugPoint(point);
#endif

	if (current_target_ == nullptr)
		return;
	auto from_xy = PinchPointOnWindow(pinch_prev_) - xy_rotation_center_;
	auto to_xy = PinchPointOnWindow(point) - xy_rotation_center_;
	auto from = Polycode::Vector3(from_xy.x, - from_xy.y, 0);
	auto to = Polycode::Vector3(to_xy.x, - to_xy.y, (pinch_prev_.z - point.z) * 2); // TODO: find way to handle z
	from.Normalize(); to.Normalize();
	auto diff = FromTwoVectors(from, to);
	Polycode::Quaternion parents, parents_inv;
	auto parent = current_target_->bone->getParentBone();
	while (parent) {
		parents = parent->getRotationQuat() * parents;
		parents_inv = parents_inv * parent->getRotationQuat().Inverse();
		parent = parent->getParentBone();
	}
	auto mesh_rot = mesh_->getRotationQuat();
	// FIXME: Camera rotation is adjusted to [1,0,0,0]
	auto new_quot = parents_inv * mesh_rot.Inverse() * diff * mesh_rot * parents * current_target_->bone->getRotationQuat();
	current_target_->bone->setRotationByQuaternion(new_quot);
	pinch_prev_ = point;
}

void BoneManipulation::OnPinchEnd() {
	if (current_target_) {
		current_target_->marker->setColor(0.8, 0.5, 0.5, 0.8);
		current_target_ = nullptr;
		pinch_end_sound_->Play();
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