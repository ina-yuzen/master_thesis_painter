#include "PinchTracker.h"

#include "CameraEventListeners.h"
#include "Context.h"
#include "DepthMap.h"

namespace mobamas {

void PinchTracker::NotifyNewData(DepthMap const& depth_map, Option<cv::Point3f> const& data) {
	auto listener = context_->pinch_listeners.lock();
	if (!listener)
		return;
	if (prev_) {
		if (data) {
			listener->OnPinchMove(*data);
		}
		else {
			listener->OnPinchEnd();
		}
	}
	else {
		if (data) {
			listener->OnPinchStart(*data);
		}
	}
	prev_ = data;
}

}
