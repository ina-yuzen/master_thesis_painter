#pragma once
#include <memory>
#include <Polycode.h>

namespace mobamas {

enum Brush {
	PEN, STAMP
};

struct Context;

class PenPicker: public Polycode::EventHandler {
public:
	PenPicker(std::shared_ptr<Context> context);
	void handleEvent(Polycode::Event *e) override;
	Brush current_brush() { return current_brush_; }
	Polycode::Color current_color() { return current_color_; }
	int current_size() { return current_size_; }
	Polycode::Texture* current_stamp() { return current_stamp_; }

private:
	static const std::vector<Polycode::Color> kPalette;
	std::shared_ptr<Context> context_;
	Polycode::Scene *scene_;
	Brush current_brush_;
	Polycode::Color current_color_;
	int current_size_;
	Polycode::Texture* current_stamp_;
	std::vector<Polycode::ScenePrimitive*> color_picks_;
	std::vector<Polycode::ScenePrimitive*> size_picks_;
	std::vector<Polycode::SceneMesh*> stamp_picks_;

	std::unique_ptr<Polycode::ScenePrimitive> cursor_;

	void PickClicked(const Polycode::Vector2& point);
	void UpdateCursorStyle();
};

}