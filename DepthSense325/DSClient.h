#pragma once
#include <memory>
#include <opencv2\opencv.hpp>

#include <DepthSense.hxx>

namespace mobamas {
	
const int kDepthWidth = 320;
const int kDepthHeight = 240;
const int kColorWidth = 640;
const int kColorHeight = 480;

struct Context;

class DSClient: public std::enable_shared_from_this<DSClient> {
public:
	typedef void (*DepthCallback)(DepthSense::DepthNode obj, DepthSense::DepthNode::NewSampleReceivedData data, std::shared_ptr<Context> context);

	static std::shared_ptr<DSClient> create() { return std::shared_ptr<DSClient>(new DSClient()); }
	~DSClient();
	bool Prepare(std::shared_ptr<Context> context, DepthCallback handlerFunc);
	void Run();
	cv::Mat LastColorData() { return last_color_data_; } 

private:
	DSClient();
	DepthSense::Context context_;
	DepthSense::DepthNode depth_;
	DepthSense::ColorNode color_;
	std::unique_ptr<DepthSense::ProjectionHelper> projHelper;
	cv::Mat last_color_data_;

	// callback
	friend void onNewColorSample(DepthSense::ColorNode node, DepthSense::ColorNode::NewSampleReceivedData data, std::shared_ptr<DSClient> ds_client);
};
}