#pragma once
#include <memory>
#include <opencv2\opencv.hpp>

namespace mobamas {

class DSClient;
class Recorder;

struct Context {
	std::shared_ptr<DSClient> ds_client;
	std::unique_ptr<Recorder> recorder;
};
}