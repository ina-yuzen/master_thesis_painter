#pragma once

#include <Polycode.h>

namespace mobamas {

class ModelRotation: Polycode::EventHandler {
public:
	ModelRotation(Polycode::SceneMesh *mesh_);
	void handleEvent(Polycode::Event *e) override;

private:
	Polycode::SceneMesh *mesh_;
	Polycode::Vector2 mouse_prev_;
	bool moving_;
};

}