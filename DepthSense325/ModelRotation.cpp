#include "ModelRotation.h"

#include <ctime>
#include <PolyWinCore.h>
#include <iostream>

#include "Context.h"

namespace mobamas {

const int kDistance = sqrt(27.0);
const double kSensitivity = 1;
const double kWheelScaleStep = 0.1;

ModelRotation::ModelRotation(std::shared_ptr<Context> context, Polycode::Entity *mesh): 
	context_(context),
	EventHandler(), 
	mesh_(mesh),
	moving_(false)
{
	auto input = Polycode::CoreServices::getInstance()->getInput();
	using Polycode::InputEvent;

	if (context->operation_mode == OperationMode::MouseMode) {
		input->addEventListener(this, InputEvent::EVENT_MOUSEDOWN);
		input->addEventListener(this, InputEvent::EVENT_MOUSEUP);
		input->addEventListener(this, InputEvent::EVENT_MOUSEMOVE);
		input->addEventListener(this, InputEvent::EVENT_MOUSEWHEEL_DOWN);
		input->addEventListener(this, InputEvent::EVENT_MOUSEWHEEL_UP);
	} else {
		input->addEventListener(this, InputEvent::EVENT_TOUCHES_BEGAN);
		input->addEventListener(this, InputEvent::EVENT_TOUCHES_MOVED);
	}
}

void ModelRotation::handleEvent(Polycode::Event *e) {
	using Polycode::InputEvent;
	
	auto rotate_by_diff = [&](Polycode::Vector2 const& new_pos) {
		auto diff = new_pos - mouse_prev_;
		mesh_->Yaw(diff.x * kSensitivity);
		mesh_->Pitch(diff.y * kSensitivity);
		mouse_prev_ = new_pos;
	};

	switch (e->getEventCode()) {
	case InputEvent::EVENT_TOUCHES_MOVED:
		if (moving_) {
			InputEvent *ie = (InputEvent*)e;
			auto touches = ie->touches;
			
			if (operation_ == Operation::ROTATE) {
				if (touches.size() < 1) {
					moving_ = false;
					return;
				}
				auto new_pos = ((InputEvent*)e)->touches.front().position;
				rotate_by_diff(new_pos);
			}
			else if (operation_ == Operation::SCALE) {
				if (touches.size() < 2) {
					moving_ = false;
					return;
				}
				auto finger_distance = touches[0].position.distance(touches[1].position);
				auto diff = (finger_distance - distance_prev_) * 0.01 + 1;
				mesh_->Scale(diff, diff, diff);
				distance_prev_ = finger_distance;
			}
		}
		break;
	case InputEvent::EVENT_TOUCHES_BEGAN:
		{
			InputEvent *ie = (InputEvent*)e;
			auto touches = ie->touches;
			switch (touches.size()) {
			case 1:
				moving_ = true;
				mouse_prev_ = touches[0].position;
				operation_ = Operation::ROTATE;
				context_->logfs << time(nullptr) << ": RotationStart" << std::endl;
				break;
			case 2:
				moving_ = true;
				distance_prev_ = touches[0].position.distance(touches[1].position);
				operation_ = Operation::SCALE;
				context_->logfs << time(nullptr) << ": ScaleStart" << std::endl;
				break;
			default:
				context_->logfs << time(nullptr) << ": " << (operation_ == Operation::ROTATE ? "Rotation" : "Scale") << "Start" << std::endl;
				moving_ = false;
				break;
			}
		}
		break;
	case InputEvent::EVENT_MOUSEDOWN:
		moving_ = ((InputEvent*)e)->getMouseButton() == 1;
		mouse_prev_ = ((InputEvent*)e)->getMousePosition();
		operation_ = Operation::ROTATE;
		break;	
	case InputEvent::EVENT_MOUSEUP:
		moving_ = false;
		break;
	case InputEvent::EVENT_MOUSEMOVE:
		if (moving_)
			rotate_by_diff(((InputEvent*)e)->getMousePosition());
		break;
	case InputEvent::EVENT_MOUSEWHEEL_UP:
		mesh_->setScale(mesh_->getScale() + Polycode::Vector3(kWheelScaleStep, kWheelScaleStep, kWheelScaleStep));
		break;
	case InputEvent::EVENT_MOUSEWHEEL_DOWN:
		mesh_->setScale(mesh_->getScale() - Polycode::Vector3(kWheelScaleStep, kWheelScaleStep, kWheelScaleStep));
		break;
	}
}


}