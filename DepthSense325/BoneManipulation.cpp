#include "BoneManipulation.h"

#include <functional>
#include <string>
#include <ctime>
#include <vector>

#include "Context.h"
#include "EditorApp.h"
#include "Import.h"
#include "Util.h"

namespace mobamas {

const int kMovableBonesDepth = 5;	
const std::map<Models, std::vector<std::wstring>> kManipulatableBones = ([] {
	std::map<Models, std::vector<std::wstring>> map;
	{
		std::vector<std::wstring> v;
		v.push_back(L"Bone.001_R.001");
		v.push_back(L"Bone.001_L.001");
		v.push_back(L"Bone_L.002");
		v.push_back(L"Bone_R.002");
		map[Models::ROBOT] = v;
	}
	{
		std::vector<std::wstring> v;
		/*v.push_back(L"ç∂îØÇP");
		v.push_back(L"âEîØÇP");
		v.push_back(L"ç∂òr");
		v.push_back(L"âEòr");
		v.push_back(L"ç∂ë´");
		v.push_back(L"âEë´");*/
		v.push_back(L"left_hair1");
		v.push_back(L"right_hair1");
		v.push_back(L"left_arm");
		v.push_back(L"right_arm");
		v.push_back(L"left_leg");
		v.push_back(L"right_leg");
		map[Models::MIKU] = v;
	}
	{
		std::vector<std::wstring> v;
		v.push_back(L"Bone002");
		map[Models::TREASURE] = v;
		}
	return map;
})();

static std::vector<Polycode::Vector3> CalculateBoneCenters(MeshGroup* group);

Polycode::ScenePrimitive* CreateHandleMarker() {
	auto marker = new Polycode::ScenePrimitive(Polycode::ScenePrimitive::TYPE_VPLANE, 0.2, 0.2);
	marker->setColor(0.8, 0.5, 0.5, 0.8);
	marker->depthTest = false;
	marker->billboardMode = true;
	return marker;
}

BoneManipulation::BoneManipulation(std::shared_ptr<Context> context, Polycode::Scene *scene, MeshGroup* mesh, Models model) :
    context_(context),
	EventHandler(),
	scene_(scene),
	mesh_(mesh),
	handles_(),
	current_target_(nullptr)
{
	auto skeleton = mesh->getSkeleton();
	if (!skeleton)
		return;
	std::map<Polycode::Bone*, unsigned int> bone_id_map;
	for (unsigned int bidx = 0; bidx < skeleton->getNumBones(); bidx++)
	{
		auto b = skeleton->getBone(bidx);
		bone_id_map[b] = bidx;
		b->disableAnimation = true;
	}
	auto centers = CalculateBoneCenters(mesh);
	for (auto name: kManipulatableBones.at(model)) {
		BoneHandle handle;
		auto bone = handle.bone = skeleton->getBoneByName(name);
		assert(bone);
		handle.marker = CreateHandleMarker();
		auto child = bone;
		while (child->getNumChildBones() > 0) {
			child = child->getChildBone(0);
		}
		// Get back, because some bones have no associated vertices.
		while (centers[bone_id_map[child]] == Polycode::Vector3(0, 0, 0)) {
			child = child->getParentBone();
		}
		handle.handle_bone_id = bone_id_map[child];
		handles_.push_back(handle);
		scene->addEntity(handle.marker);
	}

	if (context->operation_mode == OperationMode::MouseMode) {
		auto input = Polycode::CoreServices::getInstance()->getInput();
		using Polycode::InputEvent;
		input->addEventListener(this, InputEvent::EVENT_MOUSEMOVE);
		input->addEventListener(this, InputEvent::EVENT_MOUSEDOWN);
		input->addEventListener(this, InputEvent::EVENT_MOUSEUP);
		input->addEventListener(this, InputEvent::EVENT_KEYDOWN);
		input->addEventListener(this, InputEvent::EVENT_KEYUP);
	}
	pinch_start_sound_ = new Polycode::Sound("Resources/click7.wav");
	pinch_end_sound_ = new Polycode::Sound("Resources/click21.wav");
}

const int kMouseButtonCode = 0;
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
		if (ctrl_pressed_) {
			OnPinchMove(fake_3d_coord(ie));
			e->cancelEvent();
		}
		break;
	}
	case InputEvent::EVENT_MOUSEDOWN:
	{
		auto ie = (InputEvent*)e;
		if (ie->getMouseButton() == kMouseButtonCode && ctrl_pressed_) {
			OnPinchStart(fake_3d_coord(ie));
			e->cancelEvent();
		}
		break;
	}
	case InputEvent::EVENT_MOUSEUP:
	{
		auto ie = (InputEvent*)e;
		if (ie->getMouseButton() == kMouseButtonCode)
			OnPinchEnd();
		break;
	}
	case InputEvent::EVENT_KEYDOWN:
	{
		auto ie = (InputEvent*)e;
		auto key = ie->getKey();
		if (key == KEY_LCTRL || key == KEY_RCTRL)
			ctrl_pressed_ = true;
		break;
	}
	case InputEvent::EVENT_KEYUP:
	{
		auto ie = (InputEvent*)e;
		auto key = ie->getKey();
		if (key == KEY_LCTRL || key == KEY_RCTRL)
			ctrl_pressed_ = false;
		break;
	}
	}
}

// TODO: optimize this and ActualVertexPositions
// calculate only for required bone ids
static std::vector<Polycode::Vector3> CalculateBoneCenters(MeshGroup* group) {
	auto skeleton = group->getSkeleton();
	std::vector<Polycode::Vector3> bone_centers(skeleton->getNumBones(), Polycode::Vector3(0, 0, 0));
	std::vector<double> bone_weights(skeleton->getNumBones(), 0);
	for (auto mesh : group->getSceneMeshes()) {
		auto raw = mesh->getMesh();
		std::vector<Polycode::Vector3> adjusted_points = ActualVertexPositions(mesh);
		auto indices = raw->vertexBoneIndexArray;
		auto weights = raw->vertexBoneWeightArray;
		for (int vi = 0; vi < indices.getDataSize(); vi++) {
			int bi = indices.data[vi];
			double w = weights.data[vi];
			if (w > 0.0) {
				bone_centers[bi] += adjusted_points[vi / 4] * w;
				bone_weights[bi] += w;
			}
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
	auto this_pos = renderer->Project(camera, projection, view, centers[current_target->handle_bone_id]);
	auto parent_pos = renderer->Project(camera, projection, view, centers[current_target->bone->parentBoneId]);

	renderer->EndRender();

	return (parent_pos + this_pos) * 0.5;
}
void BoneManipulation::Update() {
	if (mesh_->getSkeleton() == nullptr)
		return;
	auto bone_centers = CalculateBoneCenters(mesh_);
	for (auto handle: handles_) {
		handle.marker->setPosition(bone_centers[handle.handle_bone_id]);
	}
	auto target = current_target_.load();
	if (require_xy_rotation_center_recalculation_ && target) {
		target->marker->setColor(1.0, 0.5, 0.5, 0.8);
		xy_rotation_center_ = EstimateXyRotationCenter(scene_, CalculateBoneCenters(mesh_), target);
		require_xy_rotation_center_recalculation_ = false;
	}
}

static Polycode::Vector2 PinchPointOnWindow(cv::Point3f const& point) {
	return CameraPointToScreen(point.x, point.y);
}
void BoneManipulation::OnPinchStart(cv::Point3f point) {
	pinch_prev_ = point;
	cached_points_.clear();
	for (int i = 0; i < 6; i++) {
		cached_points_.push_back(point);
	}

	auto new_target = SelectHandleByWindowCoord(PinchPointOnWindow(point), 10.0);
	current_target_.store(new_target);
	require_xy_rotation_center_recalculation_ = true; // flag for calculating after that
	if (new_target) {
		context_->logfs << time(nullptr) << ": PinchStart " << point.x << ", " << point.y << ", " << point.z << ": " << new_target->handle_bone_id << std::endl;
		pinch_start_sound_->Play();
	}
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

	auto target = current_target_.load();
	if (target == nullptr)
		return;
	cached_points_.push_back(point);
	cached_points_.pop_front();
	cv::Point3f point_new;
	for (const auto point : cached_points_) {
		point_new += point;
	}
	point_new *= (1 / 6.0f);
	auto from_xy = PinchPointOnWindow(pinch_prev_) - xy_rotation_center_;
	auto to_xy = PinchPointOnWindow(point_new) - xy_rotation_center_;
	auto from = Polycode::Vector3(from_xy.x, - from_xy.y, 0);
	auto to = Polycode::Vector3(to_xy.x, - to_xy.y, (pinch_prev_.z - point_new.z) * - 1);
	from.Normalize(); to.Normalize();
	auto diff = FromTwoVectors(from, to);
	Polycode::Quaternion parents, parents_inv;
	auto parent = target->bone->getParentBone();
	while (parent) {
		parents = parent->getRotationQuat() * parents;
		parents_inv = parents_inv * parent->getRotationQuat().Inverse();
		parent = parent->getParentBone();
	}
	auto mesh_rot = mesh_->getRotationQuat();
	// FIXME: Camera rotation is adjusted to [1,0,0,0]
	auto new_quot = parents_inv * mesh_rot.Inverse() * diff * mesh_rot * parents * target->bone->getRotationQuat();
	target->bone->setRotationByQuaternion(new_quot);
	pinch_prev_ = point_new;
	mesh_->applyBoneMotion();
}

void BoneManipulation::OnPinchEnd() {
	auto target = current_target_.load();
	if (!target)
		return;

	context_->logfs << time(nullptr) << ": PinchEnd : " << target->handle_bone_id << std::endl;
	target->marker->setColor(0.8, 0.5, 0.5, 0.8);
	current_target_.store(nullptr);
	pinch_end_sound_->Play();
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