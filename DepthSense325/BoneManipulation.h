#pragma once
#include <memory>
#include <queue>
#include <vector>

#include <Polycode.h>

#include "CameraEventListeners.h"
#include "Models.h"

namespace mobamas {

struct Context;

struct BoneHandle {
	Polycode::Bone* bone;
	Polycode::SceneMesh *marker;
	unsigned int handle_bone_id;
};

class BoneManipulation: public Polycode::EventHandler, public PinchEventListener {
public:
	BoneManipulation(std::shared_ptr<Context> context, Polycode::Scene *scene, Polycode::SceneMesh *mesh, Models model);
	void handleEvent(Polycode::Event *e) override;
	void Update();

	void OnPinchStart(cv::Point3f point);
	void OnPinchMove(cv::Point3f point);
	void OnPinchEnd();
	
private:
	std::shared_ptr<Context> context_;
	Polycode::Sound* pinch_start_sound_;
	Polycode::Sound* pinch_end_sound_;
	Polycode::Scene *scene_;
	Polycode::SceneMesh *mesh_;
	std::vector<BoneHandle> handles_;
	BoneHandle* current_target_;
	Polycode::Vector2 xy_rotation_center_;
	cv::Point3f pinch_prev_;
	volatile bool require_xy_rotation_center_recalculation_ = false;
	std::deque<cv::Point3f> *cached_points_;

	BoneHandle* SelectHandleByWindowCoord(Polycode::Vector2 point, double allowed_error = 0.1);
};

}