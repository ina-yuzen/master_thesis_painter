#include "RSClient.h"

#include <cassert>
#include <iostream>
#include <pxcsensemanager.h>
#include <pxcstatus.h>

#include "Algorithms.h"
#include "Context.h"
#include "DepthMap.h"
#include "Util.h"

namespace mobamas {

bool RSClient::Prepare() {
	auto handle_error = [&](pxcStatus st) {
		ReportPxcBadStatus(st);
		sm_->Release();
		sm_ = nullptr;
		return false;
	};

	sm_ = PXCSenseManager::CreateInstance();
	if (sm_ == nullptr) {
		return false;
	}

	pxcStatus st;
	// this enables Depth channel
	st = sm_->EnableBlob();
	if (st != PXC_STATUS_NO_ERROR)
		return handle_error(st);
	// TODO: configure blob module to achieve better results

	st = sm_->Init();
	if (st != PXC_STATUS_NO_ERROR)
		return handle_error(st);

	// Front facing
	st = sm_->QueryCaptureManager()->QueryDevice()->SetMirrorMode(PXCCapture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL);
	if (st != PXC_STATUS_NO_ERROR)
		return handle_error(st);
	
	return true;
}

/**
Find the first segmentation and return it as cv::Mat.
If not found, return empty cv::Mat.
*/
static cv::Mat FindFirstSegmentationMask(PXCBlobData* blob_data) {
	pxcStatus error;
	auto count = blob_data->QueryNumberOfBlobs();
	for (int nth = 0; nth < count; nth++)
	{
		PXCBlobData::IBlob* iblob = nullptr;
		error = blob_data->QueryBlobByAccessOrder(nth, PXCBlobData::ACCESS_ORDER_NEAR_TO_FAR, iblob);
		if (error != PXC_STATUS_NO_ERROR) {
			ReportPxcBadStatus(error);
			break;
		}
		PXCImage* seg_image;
		error = iblob->QuerySegmentationImage(seg_image);
		if (error == PXC_STATUS_DATA_UNAVAILABLE) {
			continue;
		}
		PXCImage::ImageData data;
		auto info = seg_image->QueryInfo();
		assert(info.format == PXCImage::PIXEL_FORMAT_Y8);
		seg_image->AcquireAccess(PXCImage::ACCESS_READ, &data);
		cv::Mat mat(info.height, info.width, CV_8UC1, data.planes[0]);
		seg_image->ReleaseAccess(&data);
		return mat;
	}
	return cv::Mat();
}

static cv::Mat ConvertDepthImage(PXCCapture::Sample* sample) {
	auto depth = sample->depth;
	PXCImage::ImageData data;
	auto info = depth->QueryInfo();
	assert(info.format == PXCImage::PIXEL_FORMAT_DEPTH);
	auto error = depth->AcquireAccess(PXCImage::ACCESS_READ, &data);
	assert(error == PXC_STATUS_NO_ERROR);
	assert(data.planes[0] != nullptr);
	cv::Mat mat(info.height, info.width, CV_16UC1, data.planes[0]);
	depth->ReleaseAccess(&data);
	return mat;
}

static uint16_t GetMinimumApplicableValue(cv::Mat depth) {
	assert(!depth.empty());
	uint16_t min = 0xffff;
	for (size_t i = 0, length = depth.total(); i < length; i++)
	{
		auto v = depth.at<uint16_t>(i);
		if (min > v && v > 0) min = v;
	}
	return min;
}

static DepthMap CreateDepthMap(PXCCapture::Sample* sample, cv::Mat const& raw_depth, cv::Mat const& binary, uint16_t saturated) {
	auto depth = sample->depth;
	auto info = depth->QueryInfo();	
	auto size = raw_depth.total();

#ifdef _DEBUG
	cv::Mat norm(raw_depth.size(), CV_8UC1);
	uint16_t largest = 0;
	for (size_t i = 0; i < size; i++)
	{
		auto v = raw_depth.at<uint16_t>(i);
		if (v != saturated && v > largest) largest = v;
	}
	for (size_t i = 0; i < size; i++)
	{
		uint8_t normalized;
		auto v = raw_depth.at<uint16_t>(i);
		if (v == saturated) normalized = 0;
		else normalized = 0xff - (v * 0xf0) / largest;
		norm.at<uint8_t>(i) = normalized;
	}
#else
	cv::Mat norm;
#endif
	return DepthMap{
		info.width,
		info.height,
		saturated,
		raw_depth,
		norm,
		binary
	};
}

const int kCalibrationFrames = 0; // 0 to disable calibration
void RSClient::Run() {
	pxcStatus error;

	auto blob_data = sm_->QueryBlob()->CreateOutput();
	if (blob_data == nullptr) {
		std::cout << "Failed to create Blob output in RSClient::Run()" << std::endl;
		return;
	}
	auto device = sm_->QueryCaptureManager()->QueryDevice();
	uint16_t saturated = device->QueryDepthLowConfidenceValue();
	std::cout << "Saturated value: " << saturated << std::endl;

	std::vector<uint16_t> min_values;
	min_values.reserve(kCalibrationFrames);
	uint16_t min_depth_threshold = 350; // good default value for front facing setting
	int iter_count = 0;

	while (!should_quit_ && sm_ != nullptr && (error = sm_->AcquireFrame(true)) >= PXC_STATUS_NO_ERROR) {
		error = blob_data->Update();
		if (error != PXC_STATUS_NO_ERROR)
			ReportPxcBadStatus(error);

		auto sample = sm_->QuerySample();

		auto raw_depth = ConvertDepthImage(sample);
		auto seg_mask = FindFirstSegmentationMask(blob_data);
		if (seg_mask.empty()) {
			seg_mask = cv::Mat(raw_depth.size(), CV_8UC1, cv::Scalar(0));
		}

		cv::Mat new_depth;
		raw_depth.copyTo(new_depth, seg_mask);
		auto min_depth = GetMinimumApplicableValue(new_depth);

		if (iter_count < kCalibrationFrames) {
			min_values.push_back(min_depth);
			if (iter_count == kCalibrationFrames - 1) {
				std::sort(min_values.begin(), min_values.end());
				min_depth_threshold = min_values[kCalibrationFrames / 4]; // 25 percentile to avoid too min noise
				std::cout << "Calibrated to " << min_depth_threshold << std::endl;
			}
			std::cout << min_depth << std::endl;
		}
		else {
			if (min_depth_threshold < min_depth + 10) {
				seg_mask = 0;
				new_depth = 0;
			}

			{
				std::lock_guard<std::mutex> lock(mutex_);
				segmented_depth_ = new_depth;
			}

			auto depth_map = CreateDepthMap(sample, raw_depth, seg_mask, saturated);
			auto result = PinchRightEdge(context_, depth_map);
			tracker_.NotifyNewData(depth_map, result);
		}

		iter_count++;
		sm_->ReleaseFrame();
	}

	blob_data->Release();
	if (error != PXC_STATUS_NO_ERROR) {
		ReportPxcBadStatus(error);
	}
}

void RSClient::Quit() {
	should_quit_ = true;
}

RSClient::~RSClient() {
	if (sm_ != nullptr) {
		sm_->Close();
		sm_->Release();
		sm_ = nullptr;
	}
}

}