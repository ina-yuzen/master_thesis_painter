#pragma once

#include <Polycode.h>

namespace mobamas {

class CameraRotation: Polycode::EventHandler {
public:
	CameraRotation(Polycode::Scene *scene);
	void handleEvent(Polycode::Event *e) override;

private:
	void ApplyAngle();

	Polycode::Scene *scene_;
	Polycode::Vector2 mouse_prev_;
	Polycode::Vector2 angle_;
	bool moving_;
};

}