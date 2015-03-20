#pragma once
#include <memory>
#include <Polycode.h>

namespace mobamas {

class ColorPicker: public Polycode::EventHandler {
public:
	ColorPicker();
	void handleEvent(Polycode::Event *e) override;
	Polycode::Color current_color() { return current_color_; }

private:
	static const std::vector<Polycode::Color> kPalette;
	Polycode::Scene *scene_;
	Polycode::Color current_color_;
	std::unique_ptr<Polycode::SceneEntity> cursor_;
};

}