#pragma once

#include <memory>
#include <Polycode.h>

namespace mobamas {

struct Context;

class ModelRotation: Polycode::EventHandler {
public:
	ModelRotation(std::shared_ptr<Context> context, Polycode::Entity *mesh_);
	void handleEvent(Polycode::Event *e) override;

private:

	enum Operation {
		ROTATE,
		SCALE
	};

	std::shared_ptr<Context> context_;
	Polycode::Entity *mesh_;
	Polycode::Vector2 mouse_prev_;
	double distance_prev_;
	bool moving_;
	Operation operation_;
};

}