#pragma once
#include <Polycode.h>
#include <memory>

namespace mobamas {

struct Context;
class PenPicker;

class ModelPainter : public Polycode::EventHandler {
public:
	ModelPainter(std::shared_ptr<Context> context, Polycode::Scene *scene, Polycode::Entity *mesh);
	void handleEvent(Polycode::Event *e) override;

private:
	std::shared_ptr<Context> context_;
	Polycode::Scene *scene_;
	Polycode::Entity *mesh_;
	std::unique_ptr<Polycode::Vector2> prev_tc_;
	std::unique_ptr<PenPicker> picker_;
	bool left_clicking_;
	void paintSceneMesh(Polycode::Entity*, Polycode::Ray);
};

}