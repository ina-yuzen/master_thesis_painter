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
	input->addEventListener(this, InputEvent::EVENT_TOUCHES_BEGAN);
	input->addEventListener(this, InputEvent::EVENT_TOUCHES_ENDED);
	input->addEventListener(this, InputEvent::EVENT_TOUCHES_MOVED);
}

const int kMouseRightButtonCode = 1;

void ModelRotation::handleEvent(Polycode::Event *e) {
	using Polycode::InputEvent;

	auto set_click_state = [&](bool new_val) {
		InputEvent *ie = (InputEvent*)e;
		if (new_val && ie->touches.size() == 2) {
			moving_ = new_val;
			mouse_prev_ = ie->touches.front().position;
		}
		else if (moving_ == true && !new_val) {
			moving_ = new_val; 
		}
	};

	switch (e->getEventCode()) {
	case InputEvent::EVENT_TOUCHES_MOVED:
		if (moving_) {
			auto new_pos = ((InputEvent*)e)->touches.front().position;
			auto diff = new_pos - mouse_prev_;
			mesh_->Yaw(diff.x * kSensitivity);
			mesh_->Pitch(diff.y * kSensitivity);
			mouse_prev_ = new_pos;
		}
		break;
	case InputEvent::EVENT_TOUCHES_BEGAN:
		set_click_state(true);
		break;
	case InputEvent::EVENT_TOUCHES_ENDED:
		set_click_state(false);
		break;
	}
}


}