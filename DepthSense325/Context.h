#pragma once
#include <fstream>
#include <memory>
#include <opencv2\opencv.hpp>

#include "Models.h"

namespace mobamas {

class Recorder;
class RSClient;
class PinchEventListener;

enum OperationMode {
	MouseMode,
	TouchMode,
	MidAirMode,
};

struct Context {
	Models model;
	OperationMode operation_mode;
	std::fstream logfs;
	std::unique_ptr<Recorder> recorder;
	std::shared_ptr<RSClient> rs_client;
	std::weak_ptr<PinchEventListener> pinch_listeners;
};

}