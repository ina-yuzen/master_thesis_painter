#pragma once
#include <memory>
#include <opencv2\opencv.hpp>

namespace mobamas {

class DSClient;
class Recorder;
class Background;

struct Context {
	std::shared_ptr<DSClient> ds_client;
	std::unique_ptr<Recorder> recorder;
	std::unique_ptr<Background> background;
};
}