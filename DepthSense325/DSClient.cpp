#include "DSClient.h"

namespace mobamas {

static void configureDepthNode(DepthSense::DepthNode node, 
							   DSClient::DepthCallback handlerFunc,
							   std::shared_ptr<Context> context)
{
	node.newSampleReceivedEvent().connect(handlerFunc, context);

	auto config = node.getConfiguration();
    config.frameFormat = DepthSense::FRAME_FORMAT_QVGA;
    config.framerate = 25;
    config.mode = DepthSense::DepthNode::CAMERA_MODE_CLOSE_MODE;
    config.saturation = true;

	node.setEnableVertices(true);
	node.setEnableDepthMap(true);
	node.setEnableUvMap(true);

	node.setConfiguration(config);
}

// New color sample event handler
void onNewColorSample(DepthSense::ColorNode node, DepthSense::ColorNode::NewSampleReceivedData data, std::shared_ptr<DSClient> ds_client)
{
	ds_client->last_color_data_ = cv::Mat(kColorHeight, kColorWidth, CV_8UC3, (void*)(const uint8_t*)data.colorMap);
}

static void configureColorNode(DepthSense::ColorNode& node, std::shared_ptr<DSClient> client)
{
    // connect new color sample handler
	node.newSampleReceivedEvent().connect(&onNewColorSample, client);

    auto config = node.getConfiguration();
    config.frameFormat = DepthSense::FRAME_FORMAT_VGA;
	config.compression = DepthSense::COMPRESSION_TYPE_MJPEG;
    config.framerate = 25;

    node.setEnableColorMap(true);
	node.setWhiteBalanceAuto(false);

	node.setConfiguration(config);
}

DSClient::DSClient(): context_(DepthSense::Context::create("localhost")) {
}

bool DSClient::Prepare(std::shared_ptr<Context> context, DepthCallback handlerFunc) {
    // Get the list of currently connected devices
	auto da = context_.getDevices();

    // We are only interested in the first device
    if (da.size() < 1) {
		std::cout << "Could not find Device" << std::endl;
		return false;
	}
	
	auto na = da[0].getNodes();
	std::cout << "Found " << na.size() << "nodes" << std::endl;
        
	
	for (size_t n = 0; n < na.size(); n++) {
		auto node = na[n];
		context_.requestControl(node, 0);
		if (node.is<DepthSense::DepthNode>() && !depth_.isSet())
		{
			depth_ = node.as<DepthSense::DepthNode>();
			configureDepthNode(depth_, handlerFunc, context);
			context_.registerNode(node);
		}
		
		if (node.is<DepthSense::ColorNode>() && !color_.isSet())
		{
			color_ = node.as<DepthSense::ColorNode>();
			configureColorNode(color_, shared_from_this());
			context_.registerNode(node);
		}
    }

	if (!depth_.isSet()) {
		std::cout << "Could not find Depth node" << std::endl;
		return false;
	} 
	if (!color_.isSet()) {
		std::cout << "Could not find Color node" << std::endl;
		return false;
	}
	return true;
}

void DSClient::Run() {
	context_.startNodes();
	context_.run();
	context_.stopNodes();
}

DSClient::~DSClient() {
    if (color_.isSet()) context_.unregisterNode(color_);
    if (depth_.isSet()) context_.unregisterNode(depth_);
}

}  // namespace mobamas