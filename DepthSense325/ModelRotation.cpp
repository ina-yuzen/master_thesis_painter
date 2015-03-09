#include "ModelRotation.h"

namespace mobamas {

const int kDistance = sqrt(27.0);
const double kSensitivity = 1;

ModelRotation::ModelRotation(Polycode::SceneMesh *mesh): 
	EventHandler(), 
	mesh_(mesh),
	moving_(false)
{
	auto input = Polycode::CoreServices::getInstance()->getInput();
	using Polycode::InputEvent;
	input->addEventListener(this, InputEvent::EVENT_MOUSEMOVE);
	input->addEventListener(this, InputEvent::EVENT_MOUSEDOWN);
	input->addEventListener(this, InputEvent::EVENT_MOUSEUP);
}

const int kMouseRightButtonCode = 1;

void ModelRotation::handleEvent(Polycode::Event *e) {
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
			mesh_->Yaw(diff.x * kSensitivity);
			mesh_->Pitch(diff.y * kSensitivity);
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