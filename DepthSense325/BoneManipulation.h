#pragma once
#include <atomic>
#include <memory>
#include <queue>
#include <vector>

#include <Polycode.h>

#include "CameraEventListeners.h"
#include "Models.h"

namespace mobamas {

struct Context;
class MeshGroup;

struct BoneHandle {
	Polycode::Bone* bone;
	Polycode::SceneMesh *marker;
	unsigned int handle_bone_id;
};

struct MouseTiming {
	Polycode::Vector2 position;
	int timestamp;
};

class BoneManipulation: public Polycode::EventHandler, public PinchEventListener {
public:
	BoneManipulation(std::shared_ptr<Context> context, Polycode::Scene *scene, MeshGroup *mesh, Models model);
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
	MeshGroup *mesh_;
	std::vector<BoneHandle> handles_;
	std::atomic<BoneHandle*> current_target_; // manipulated from different thread in RealSense mode
	Polycode::Vector2 pinch_offset;
	Polycode::Vector2 xy_rotation_center_;
	cv::Point3f pinch_prev_;
	volatile bool require_xy_rotation_center_recalculation_ = false;
	volatile bool ctrl_pressed_ = false;
	std::deque<cv::Point3f> cached_points_;
	MouseTiming down_timing_;

	BoneHandle* SelectHandleByWindowCoord(Polycode::Vector2 point, double allowed_error = 0.1);
	void BoneManipulation::RotateBy(Polycode::Quaternion const& q);
};

}