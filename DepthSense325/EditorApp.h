#pragma once
#include <memory>
#include <Polycode.h>
#include <PolycodeView.h>

namespace mobamas {

	const int kWinHeight = 540;// 1920 / 1.5;
	const int kWinWidth = 800;// 1080 / 1.5;

struct Context;
class BoneManipulation;
class HandVisualization;
class MeshGroup;
class ModelRotation;
class ModelPainter;

class EditorApp {
public:
	EditorApp(Polycode::PolycodeView *view, std::shared_ptr<Context> context);
	~EditorApp();
	bool Update();
	void Shutdown();

private:
	Polycode::Core *core_;
	MeshGroup* mesh_;
	MeshGroup* mihon_;
	std::shared_ptr<Context> context_;
	std::shared_ptr<BoneManipulation> bone_manipulation_;
	std::unique_ptr<HandVisualization> hand_visualization_;
	std::unique_ptr<ModelRotation> rotation_;
	std::unique_ptr<ModelPainter> painter_;
	unsigned int last_save_tick_ = 0;
};

}