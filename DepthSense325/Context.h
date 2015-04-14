#pragma once
#include <memory>
#include <opencv2\opencv.hpp>

#include "Models.h"

namespace mobamas {

class Recorder;
class RSClient;
class PinchEventListener;

struct Context {
	Models model;
	std::unique_ptr<Recorder> recorder;
	std::shared_ptr<RSClient> rs_client;
	std::weak_ptr<PinchEventListener> pinch_listeners;
};

}