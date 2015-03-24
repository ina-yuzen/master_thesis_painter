#include "RSClient.h"

#include <cassert>
#include <iostream>
#include <pxcsensemanager.h>
#include <pxcstatus.h>

#include "RSUtils.h"

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
			return cv::Mat();
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
	depth->AcquireAccess(PXCImage::ACCESS_READ, &data);
	cv::Mat mat(info.height, info.width, CV_16UC1, data.planes[0]);
	depth->ReleaseAccess(&data);
	return mat;
}

void RSClient::Run() {
	pxcStatus error;

	auto blob_data = sm_->QueryBlob()->CreateOutput();
	if (blob_data == nullptr) {
		std::cout << "Failed to create Blob output in RSClient::Run()" << std::endl;
		return;
	}

	while (!should_quit_ && sm_ != nullptr && (error = sm_->AcquireFrame(true)) >= PXC_STATUS_NO_ERROR) {
		error = blob_data->Update();
		if (error != PXC_STATUS_NO_ERROR)
			ReportPxcBadStatus(error);

		cv::Mat seg_mask = FindFirstSegmentationMask(blob_data);

		cv::Mat new_depth = cv::Mat();
		if (!seg_mask.empty()) {
			auto sample = sm_->QuerySample();
			cv::Mat raw_depth = ConvertDepthImage(sample);
			raw_depth.copyTo(new_depth, seg_mask);
		}
		{
			std::lock_guard<std::mutex> lock(mutex_);
			segmented_depth_ = new_depth;
		}

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