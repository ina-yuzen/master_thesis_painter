#pragma once
#include <memory>
#include <Polycode.h>

namespace mobamas {

class PenPicker: public Polycode::EventHandler {
public:
	PenPicker();
	void handleEvent(Polycode::Event *e) override;
	Polycode::Color current_color() { return current_color_; }
	int current_size() { return current_size_; }

private:
	static const std::vector<Polycode::Color> kPalette;
	Polycode::Scene *scene_;
	Polycode::Color current_color_;
	int current_size_;
	std::vector<Polycode::ScenePrimitive*> color_picks_;
	std::vector<Polycode::ScenePrimitive*> size_picks_;

	std::unique_ptr<Polycode::ScenePrimitive> cursor_;

	void PickClicked(const Polycode::Vector2& point);
};

}