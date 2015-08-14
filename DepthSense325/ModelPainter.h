#pragma once
#include <opencv2\opencv.hpp>
#include <Polycode.h>
#include <memory>

namespace mobamas {

struct Context;
struct Intersection;
class MeshGroup;
class PenPicker;

class ModelPainter : public Polycode::EventHandler {
public:
	ModelPainter(std::shared_ptr<Context> context, Polycode::Scene *scene, MeshGroup *mesh);
	void handleEvent(Polycode::Event *e) override;

private:
	std::shared_ptr<Context> context_;
	Polycode::Scene *scene_;
	MeshGroup *mesh_;
	std::unique_ptr<Polycode::Vector2> prev_mouse_pos_;
	std::unique_ptr<PenPicker> picker_;
	cv::Mat canvas_;
	bool left_clicking_;
	void PrepareCanvas(Polycode::Vector2 const& mouse_pos);
	void PaintTexture(Intersection const& intersection);
};

}