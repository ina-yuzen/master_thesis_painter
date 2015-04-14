#pragma once
#include <Polycode.h>
#include <vector>
#include <queue>

#include "CameraEventListeners.h"

namespace mobamas {

struct BoneHandle {
	Polycode::Bone* bone;
	Polycode::SceneMesh *marker;
	unsigned int handle_bone_id;
};

class BoneManipulation: public Polycode::EventHandler, public PinchEventListener {
public:
	BoneManipulation(Polycode::Scene *scene, Polycode::SceneMesh *mesh);
	void handleEvent(Polycode::Event *e) override;
	void Update();

	void OnPinchStart(cv::Point3f point);
	void OnPinchMove(cv::Point3f point);
	void OnPinchEnd();
	
private:
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