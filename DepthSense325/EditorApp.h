#pragma once
#include <memory>
#include <Polycode.h>
#include <PolycodeView.h>
#include "ColorPicker.h"

namespace mobamas {

const int kWinHeight = 480;
const int kWinWidth = 640;

class EditorApp: public Polycode::EventHandler {
public:
	EditorApp(Polycode::PolycodeView *view);
	~EditorApp();
	bool Update();
	void EditorApp::handleEvent(Event *e) override;

private:
	Polycode::Core *core_;
	Polycode::Scene *scene_;
	Polycode::SceneMesh *mesh_;
	std::unique_ptr<ColorPicker> picker_;
	bool left_clicking_;
};

}