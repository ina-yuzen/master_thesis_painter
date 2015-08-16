#pragma once

#include <Polycode.h>

namespace mobamas {

extern HWND hWnd;

class PenAsMouse : public Polycode::EventHandler {
public:
	PenAsMouse(Polycode::EventHandler* base);
	void handleEvent(Polycode::Event* e) override;

private:
	Polycode::EventHandler* base_;
};

}