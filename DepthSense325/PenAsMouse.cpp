#include "PenAsMouse.h"

#include <memory>

using Polycode::InputEvent;

namespace mobamas {

HWND hWnd;

PenAsMouse::PenAsMouse(Polycode::EventHandler* base) : base_(base) {
	auto input = Polycode::CoreServices::getInstance()->getInput();

	input->addEventListener(this, InputEvent::EVENT_MOUSEMOVE);
	input->addEventListener(this, InputEvent::EVENT_MOUSEDOWN);
	input->addEventListener(this, InputEvent::EVENT_MOUSEUP);
	input->addEventListener(this, InputEvent::EVENT_TOUCHES_BEGAN);
	input->addEventListener(this, InputEvent::EVENT_TOUCHES_MOVED);
	input->addEventListener(this, InputEvent::EVENT_TOUCHES_ENDED);
}

void PenAsMouse::handleEvent(Polycode::Event* e) {
	auto ie = static_cast<Polycode::InputEvent*>(e);
	auto code = e->getEventCode();
	if (code == InputEvent::EVENT_TOUCHES_BEGAN ||
		code == InputEvent::EVENT_TOUCHES_MOVED ||
		code == InputEvent::EVENT_TOUCHES_ENDED) {
		if (ie->touch.type == Polycode::TouchInfo::TYPE_PEN) {
			POINT lt;
			lt.x = 0;
			lt.y = 0;
			ClientToScreen(hWnd, &lt);
			ie->mousePosition = ie->touch.position - Polycode::Vector2(lt.x, lt.y);
			switch (code) {
			case InputEvent::EVENT_TOUCHES_BEGAN:
				e->setEventCode(InputEvent::EVENT_MOUSEDOWN);
				ie->mouseButton = Polycode::CoreInput::MOUSE_BUTTON1;
				break;
			case InputEvent::EVENT_TOUCHES_MOVED:
				e->setEventCode(InputEvent::EVENT_MOUSEMOVE);
				break;
			case InputEvent::EVENT_TOUCHES_ENDED:
				e->setEventCode(InputEvent::EVENT_MOUSEUP);
				ie->mouseButton = Polycode::CoreInput::MOUSE_BUTTON1;
				break;
			default:
				break;
			}
			base_->handleEvent(e);
		}
	} else {
		base_->handleEvent(e);
	}
}

}
