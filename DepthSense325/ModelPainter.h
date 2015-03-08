#pragma once
#include <Polycode.h>
#include <memory>

namespace mobamas {

class ColorPicker;

class ModelPainter : public Polycode::EventHandler {
public:
	ModelPainter(Polycode::Scene *scene, Polycode::SceneMesh *mesh);
	void handleEvent(Polycode::Event *e) override;

private:
	Polycode::Scene *scene_;
	Polycode::SceneMesh *mesh_;
	std::unique_ptr<ColorPicker> picker_;
	bool left_clicking_;
};

}