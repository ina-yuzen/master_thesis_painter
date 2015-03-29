#pragma once
#include <Polycode.h>
#include <vector>

#include "CameraEventListeners.h"

namespace mobamas {

struct BoneHandle {
	Polycode::Bone* bone;
	Polycode::SceneMesh *marker;
	unsigned int bone_id;
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
	Polycode::Scene *scene_;
	Polycode::SceneMesh *mesh_;
	std::vector<BoneHandle> handles_;
	BoneHandle* current_target_;
	Polycode::Vector2 xy_rotation_center_;
	cv::Point3f pinch_prev_;

	BoneHandle* SelectHandleByWindowCoord(Polycode::Vector2 point, double allowed_error = 0.1);
};

}