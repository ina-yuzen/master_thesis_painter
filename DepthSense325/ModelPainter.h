#pragma once

#include <opencv2\opencv.hpp>
#include <Polycode.h>
#include <atomic>
#include <memory>
#include <thread>

namespace mobamas {

struct Context;
struct Intersection;
class MeshGroup;
class PenAsMouse;
class PenPicker;
class PaintWorker;

class ModelPainter : public Polycode::EventHandler {
public:
	ModelPainter(std::shared_ptr<Context> context, Polycode::Scene *scene, MeshGroup *mesh, PenPicker* picker);
	void handleEvent(Polycode::Event *e) override;
	void Update();
	void Shutdown();

private:
	std::shared_ptr<Context> context_;
	std::unique_ptr<PaintWorker> worker_;
	std::unique_ptr<PenAsMouse> delegator_;
	std::thread worker_thread_;
	std::atomic_bool inturrupted_;
	bool left_clicking_;
};

}