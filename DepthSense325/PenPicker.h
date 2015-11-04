#pragma once
#include <memory>
#include <Polycode.h>

namespace mobamas {

enum Brush {
	PEN, STAMP
};

enum PenTarget {
	DRAW, BONE
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
	PenTarget pen_target() { return pen_target_; }

	static int DisplaySize(int size) {
		return (size + 1) * 5;
	}

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
	PenTarget pen_target_ = PenTarget::DRAW;
	Polycode::SceneMesh* pen_target_pick_;
	Polycode::Texture* pen_target_textures_[2];

	std::unique_ptr<Polycode::ScenePrimitive> cursor_;

	bool PickClicked(const Polycode::Vector2& point);
	void UpdateCursorStyle();
};

}