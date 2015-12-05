#include "PinchTracker.h"

#include "CameraEventListeners.h"
#include "Context.h"
#include "DepthMap.h"

namespace mobamas {

	void PinchTracker::NotifyNewData(Option<cv::Point3f> const& data) {
		auto listener = context_->pinch_listeners.lock();
		if (!listener)
			return;
		Push(data);
		if (pinching_) {
			auto point = Pop();
			if (point) {
				listener->OnPinchMove(*point);
			}
			else {
				pinching_ = false;
				listener->OnPinchEnd();
			}
		}
		else {
			auto point = PopDense();
			if (point) {
				pinching_ = true;
				listener->OnPinchStart(*point);
			}
		}

	}
	
	Option<cv::Point3f> PinchTracker::Pop(){
		auto it = prev_.begin();
		*it++;
		while (it != prev_.end()) {
			if (*it) return *it;
			*it++;
		}
		return Option<cv::Point3f>::None();
	}

	

	void PinchTracker::Push(Option<cv::Point3f> point) {
		prev_.push_back(point);
		if (prev_.size() > 6) {
			prev_.pop_front();
		}
	}

	Option<cv::Point3f> PinchTracker::PopDense() {
		auto it = prev_.begin();
		int count = 0;
		while (it != prev_.end()) {
			if (!*it++) return Option<cv::Point3f>::None();
		}
		return prev_.back();
	}
}