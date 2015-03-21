#pragma once
#include <Polycode.h>
#include <memory>

namespace mobamas {

class PenPicker;

class ModelPainter : public Polycode::EventHandler {
public:
	ModelPainter(Polycode::Scene *scene, Polycode::SceneMesh *mesh);
	void handleEvent(Polycode::Event *e) override;

private:
	Polycode::Scene *scene_;
	Polycode::SceneMesh *mesh_;
	std::unique_ptr<Polycode::Vector2> prev_tc_;
	std::unique_ptr<PenPicker> picker_;
	bool left_clicking_;
};

}