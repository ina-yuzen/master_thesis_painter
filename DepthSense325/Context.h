#pragma once
#include <memory>
#include <opencv2\opencv.hpp>

namespace mobamas {

class DSClient;
class Recorder;
class PinchEventListener;

struct Context {
	std::shared_ptr<DSClient> ds_client;
	std::unique_ptr<Recorder> recorder;
	std::weak_ptr<PinchEventListener> pinch_listeners;
	std::vector<cv::Point> prev_pinches;
};
}