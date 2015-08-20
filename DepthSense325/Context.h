#pragma once
#include <fstream>
#include <memory>
#include <opencv2\opencv.hpp>

#include "Models.h"

namespace mobamas {

class RSClient;
class PinchEventListener;
class Writer;

enum OperationMode {
	MouseMode,
	TouchMode,
	MidAirMode,
};

struct Context {
	Models model;
	OperationMode operation_mode;
	std::shared_ptr<RSClient> rs_client;
	std::weak_ptr<PinchEventListener> pinch_listeners;
	std::unique_ptr<Writer> writer;
};

}