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

	kX = 1.0E-2, kY = 1.0E-2, kYOffset = 0.7;
	kZFar = 1200;

	auto input = Polycode::CoreServices::getInstance()->getInput();
	using Polycode::InputEvent;
	if (context_->operation_mode == OperationMode::FrontMode) {
		input->addEventListener(this, InputEvent::EVENT_KEYDOWN);
		input->addEventListener(this, InputEvent::EVENT_KEYUP);
	}

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
void RSClient::handleEvent(Polycode::Event *e) {
	using Polycode::InputEvent;
	float kxyunit = 1.0E-3;
	float offsetunit = 0.1;
	uint16_t zfarunit = 100;
	switch (e->getEventCode()) {
	case InputEvent::EVENT_KEYDOWN:
	{
		auto ie = (InputEvent*)e;
		auto key = ie->getKey();
		switch (key) {
			case Polycode::PolyKEY::KEY_z:
			{
				kX += kxyunit;
				break;
			}
			case Polycode::PolyKEY::KEY_x:
			{
				kX -= kxyunit;
				break;
			}
			case Polycode::PolyKEY::KEY_c:
			{
				kY += kxyunit;
				break;
			}
			case Polycode::PolyKEY::KEY_v:
			{
				kY -= kxyunit;
				break;
			}
			case Polycode::PolyKEY::KEY_b:
			{
				kYOffset += offsetunit;
				break;
			}
			case Polycode::PolyKEY::KEY_n:
			{
				kYOffset -= offsetunit;
				break;
			}
			case Polycode::PolyKEY::KEY_m:
			{
				kZFar += zfarunit;
				break;
			}
			case Polycode::PolyKEY::KEY_COMMA:
			{
				kZFar-= zfarunit;
				break;
			}
		}
		switch (key) {
			case Polycode::PolyKEY::KEY_z:
			case Polycode::PolyKEY::KEY_x:
			case Polycode::PolyKEY::KEY_c:
			case Polycode::PolyKEY::KEY_v:
			case Polycode::PolyKEY::KEY_b:
			case Polycode::PolyKEY::KEY_n:
			case Polycode::PolyKEY::KEY_m:
			case Polycode::PolyKEY::KEY_COMMA:
			{
				std::cout << "kX: " << kX << ", kY: " << kY << ", kYOffset: " << kYOffset << ", kZFar: " << kZFar << std::endl;
				break;
			}
		}
		break;
	}
	}
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

static void ReplaceFrontalOrigin(cv::Mat& raw_depth, cv::Mat& seg_mask, cv::Point& offset, uint16_t saturated, float kX, float kY, float kYOffset, uint16_t kZFar) {
	assert(!raw_depth.empty());
	assert(raw_depth.size() == seg_mask.size());
	int cx = raw_depth.cols / 2, cy = raw_depth.rows / 2;
	offset = cv::Point(raw_depth.cols / 2, raw_depth.rows / 2);

	cv::Mat new_depth(raw_depth.rows * 2, raw_depth.cols * 2, raw_depth.type());
	cv::Point max(new_depth.cols - offset.x - 1, new_depth.rows - offset.y - 1);
	cv::Mat new_mask(new_depth.rows, new_depth.cols, seg_mask.type());
	new_depth = saturated;
	new_mask = 0;
	for (size_t y = 0; y < raw_depth.rows - 1; y++) {
		for (size_t x = 0; x < raw_depth.cols - 1; x++) {
			auto z = raw_depth.at<uint16_t>(y, x);
			auto z2 = raw_depth.at<uint16_t>(y+1, x+1);
			if (z == saturated || z2 == saturated)
				continue;
			cv::Point ps(cx + kX * (static_cast<int>(x) - cx) * z, cy + kY * (static_cast<int>(y) - cy) * z - kYOffset * raw_depth.rows);
			cv::Point pe(cx + kX * (static_cast<int>(x)+1 - cx) * z2, cy + kY * (static_cast<int>(y)+1 - cy) * z2 - kYOffset * raw_depth.rows);
			if (pe.x < -offset.x || pe.y < -offset.y
				|| ps.x > max.x || ps.y > max.y
				|| ps.x > pe.x || ps.y > pe.y)
				continue;
			for (int ix = std::max(ps.x, -offset.x); ix <= std::min(pe.x, max.x); ++ix) {
				for (int iy = std::max(ps.y, -offset.y); iy <= std::min(pe.y, max.y); ++iy) {
					new_depth.at<uint16_t>(iy + offset.y, ix + offset.x) = std::max(kZFar - z, 0);
					new_mask.at<uint8_t>(iy + offset.y, ix + offset.x) = seg_mask.at<uint8_t>(y, x);
				}
			}
		}
	}
	raw_depth = new_depth;
	seg_mask = new_mask;
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

static DepthMap CreateDepthMap(PXCCapture::Sample* sample, cv::Mat const& raw_depth, cv::Mat const& binary, cv::Point const& offset, uint16_t saturated) {
	auto depth = sample->depth;
	auto info = depth->QueryInfo();	

#ifdef _DEBUG
	auto size = raw_depth.total();
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
		binary,
		offset
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
		cv::Point offset;
		if (seg_mask.empty()) {
			seg_mask = cv::Mat(raw_depth.size(), CV_8UC1, cv::Scalar(0));
		}
		if (context_->operation_mode == OperationMode::FrontMode) {
			ReplaceFrontalOrigin(raw_depth, seg_mask, offset, saturated, kX, kY, kYOffset, kZFar);
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
			if (context_->operation_mode != OperationMode::FrontMode && min_depth_threshold < min_depth + 10) {
				seg_mask = 0;
				new_depth = 0;
			}

			auto depth_map = CreateDepthMap(sample, raw_depth, seg_mask, offset, saturated);
			std::cout << "start" << std::endl;
			/*{
				IplImage *writeTo = cvCreateImage(cvSize(1000, 1000), IPL_DEPTH_8U, 3);
				int xstep = depth_map.raw_mat.cols / 1000;
				int ystep = depth_map.raw_mat.rows / 1000;
				for (size_t y = 0; y < 1000; y++)
				{
					for (size_t x = 0; x < 1000; x++)
					{
						writeTo->imageData[y * 1000 + x] = (depth_map.raw_mat.at<uint16_t>(y * ystep, x * xstep) >> 8);
					}
				}
				cvShowImage("raw", writeTo);
				cvWaitKey(1);
				cvReleaseImage(&writeTo);
			}*/
			{
				std::lock_guard<std::mutex> lock(mutex_);
				last_depth_map_ = depth_map;
			}

			auto result = PinchRightEdge(context_, depth_map);
			tracker_.NotifyNewData(result);
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