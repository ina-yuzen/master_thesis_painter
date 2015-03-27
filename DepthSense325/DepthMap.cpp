#include "DepthMap.h"

#include "CameraEventListeners.h"
#include "Context.h"
#include "Recorder.h"
#include "Option.h"

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
	auto founds = PinchRightEdge(context, map);
	NotifyPinchChange(context, map, founds);

	auto key = cv::waitKey(1);
	if (key == 0x20)
		context->recorder->toggleSampleImage();
}

static Option<cv::Point3f> AbsolutePoint3D(const DepthMap& depth_map, const cv::Point& depth_point) {
	auto uv_map = depth_map.data.uvMap;
	int offset = depth_point.y * depth_map.w + depth_point.x;
	assert(uv_map.size() > offset);
	auto uv = uv_map[offset];
	auto depth_mm = depth_map.raw_mat.at<int16_t>(depth_point);
	if (uv.u > -1e10 && uv.v > -1e10 && depth_mm < kSaturatedDepth) {
		return Option<cv::Point3f>(cv::Point3f(uv.u, uv.v, depth_mm));
	}
	else {
		return Option<cv::Point3f>::None();
	}
}

void NotifyPinchChange(std::shared_ptr<Context> context, const DepthMap& depth_map, const std::vector<cv::Point>& founds) {
	auto listener = context->pinch_listeners.lock();
	if (!listener)
		return;
	if (context->prev_pinches.empty()) {
		if (!founds.empty()) {
			auto p = AbsolutePoint3D(depth_map, founds.front());
			if (p)
				listener->OnPinchStart(*p);
		}
	}
	else {
		if (founds.empty()) 
			listener->OnPinchEnd();
		else {
			auto p = AbsolutePoint3D(depth_map, founds.front());
			if (p)
				listener->OnPinchMove(*p);
		}
	}
	context->prev_pinches.assign(founds.begin(), founds.end());
}

}