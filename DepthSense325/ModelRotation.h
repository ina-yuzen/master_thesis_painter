#pragma once

#include <Polycode.h>

namespace mobamas {

class ModelRotation: Polycode::EventHandler {
public:
	ModelRotation(Polycode::SceneMesh *mesh_);
	void handleEvent(Polycode::Event *e) override;

private:

	enum Operation {
		ROTATE,
		SCALE
	};

	Polycode::SceneMesh *mesh_;
	Polycode::Vector2 mouse_prev_;
	double distance_prev_;
	bool moving_;
	Operation operation_;
};

}