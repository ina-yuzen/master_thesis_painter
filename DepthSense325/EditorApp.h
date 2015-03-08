#pragma once
#include <memory>
#include <Polycode.h>
#include <PolycodeView.h>

namespace mobamas {

const int kWinHeight = 480;
const int kWinWidth = 640;

class CameraRotation;
class ModelPainter;

class EditorApp {
public:
	EditorApp(Polycode::PolycodeView *view);
	~EditorApp();
	bool Update();

private:
	Polycode::Core *core_;
	Polycode::SceneMesh *mesh_;
	std::unique_ptr<CameraRotation> rotation_;
	std::unique_ptr<ModelPainter> painter_;
};

}