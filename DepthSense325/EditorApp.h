#pragma once
#include <Polycode.h>
#include <PolycodeView.h>

namespace mobamas {

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
	bool left_clicking_;
};

}