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
class PenPicker;
class PaintWorker;

class ModelPainter : public Polycode::EventHandler {
public:
	ModelPainter(std::shared_ptr<Context> context, Polycode::Scene *scene, MeshGroup *mesh);
	void handleEvent(Polycode::Event *e) override;
	void Update();
	void Shutdown();

private:
	std::shared_ptr<Context> context_;
	std::unique_ptr<PaintWorker> worker_;
	std::thread worker_thread_;
	std::atomic_bool inturrupted_;
	bool left_clicking_;
};

}