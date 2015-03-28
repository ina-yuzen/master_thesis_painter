#pragma once
#include <memory>
#include <opencv2\opencv.hpp>

namespace mobamas {

class Recorder;
class RSClient;
class PinchEventListener;

struct Context {
	std::unique_ptr<Recorder> recorder;
	std::shared_ptr<RSClient> rs_client;
	std::weak_ptr<PinchEventListener> pinch_listeners;
	std::vector<cv::Point> prev_pinches;
};
}