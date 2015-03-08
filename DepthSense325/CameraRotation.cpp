#include "CameraRotation.h"

namespace mobamas {

const int kDistance = sqrt(27.0);
const double kSensitivity = 2 * PI / 320;

CameraRotation::CameraRotation(Polycode::Scene *scene): 
	EventHandler(), 
	scene_(scene),
	angle_(PI / 4, PI / 4),
	moving_(false)
{
	ApplyAngle();

	auto input = Polycode::CoreServices::getInstance()->getInput();
	using Polycode::InputEvent;
	input->addEventListener(this, InputEvent::EVENT_MOUSEMOVE);
	input->addEventListener(this, InputEvent::EVENT_MOUSEDOWN);
	input->addEventListener(this, InputEvent::EVENT_MOUSEUP);
}

void CameraRotation::ApplyAngle() {
	auto cam = scene_->getActiveCamera();
	double horizontal = kDistance * cos(angle_.y);
	cam->setPosition(
		horizontal * cos(angle_.x),
		kDistance * sin(angle_.y),
		horizontal * sin(angle_.x));
	cam->lookAt(Polycode::Vector3(0, 1, 0));
}

const int kMouseRightButtonCode = 1;

void CameraRotation::handleEvent(Polycode::Event *e) {
	using Polycode::InputEvent;

	auto set_click_state = [&](bool new_val) {
		InputEvent *ie = (InputEvent*)e;
		if (ie->mouseButton == kMouseRightButtonCode) {
			moving_ = new_val;
			mouse_prev_ = ie->mousePosition;
		}
	};

	switch (e->getEventCode()) {
	case InputEvent::EVENT_MOUSEMOVE:
		if (moving_) {
			auto new_pos = ((InputEvent*)e)->mousePosition;
			auto diff = new_pos - mouse_prev_;
			angle_ += diff * kSensitivity;
			ApplyAngle();
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


}