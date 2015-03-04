#include "DepthMap.h"

#include "Context.h"
#include "Recorder.h"

#include "Algorithms.h"

namespace mobamas{

void onNewDepthSample(DepthSense::DepthNode node, DepthSense::DepthNode::NewSampleReceivedData data, std::shared_ptr<Context> context)
{
	DepthMap map;

	DepthSense::FrameFormat_toResolution(data.captureConfiguration.frameFormat,&map.w,&map.h);
	auto dp = (const int16_t *)data.depthMap;
	cv::Mat mat(map.h, map.w, CV_8UC1), raw(map.h, map.w, CV_16SC1, (void*)dp);

	auto size = data.depthMap.size();
	int16_t largest = 0;
	for (int i = 0; i < size; i++)
	{
		if (dp[i] < kSaturatedDepth && dp[i] > largest) largest = dp[i];
	}
	for (int i = 0; i < size; i++)
	{
		uint8_t normalized;
		if (dp[i] >= kSaturatedDepth) normalized = 0;
		else normalized = 0xff - (dp[i] * 0xf0) / largest;
		mat.at<uint8_t>(i) = normalized;
	}

	map.raw_mat = raw;
	map.normalized = mat;
	map.data = data;

	// Write processes here
	PinchNailColor(context, map);

	auto key = cv::waitKey(1);
	if (key == 0x20)
		context->recorder->toggleSampleImage();
}

}