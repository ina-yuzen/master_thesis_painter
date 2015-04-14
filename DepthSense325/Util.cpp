#include "Util.h"

#include <iostream>
#include "DepthMap.h"
#include "EditorApp.h"

namespace mobamas {

	std::vector<Polycode::Vector3> ActualVertexPositions(Polycode::SceneMesh *mesh) {
		std::vector<Polycode::Vector3> result;
		auto raw = mesh->getMesh();
		result.reserve(raw->getVertexCount());
		auto mesh_transform = mesh->getTransformMatrix();
		for (int vidx = 0; vidx < raw->vertexPositionArray.data.size() / 3; vidx++) {
			auto rest_vert = Polycode::Vector3(raw->vertexPositionArray.data[vidx * 3],
				raw->vertexPositionArray.data[vidx * 3 + 1],
				raw->vertexPositionArray.data[vidx * 3 + 2]);
			Polycode::Vector3 real_pos;
			for (int b = 0; b < 4; b++)
			{
				auto bidx = vidx * 4 + b;
				auto weight = raw->vertexBoneWeightArray.data[bidx];
				if (weight > 0.0) {
					auto bone = mesh->getSkeleton()->getBone(raw->vertexBoneIndexArray.data[bidx]);
					real_pos += bone->finalMatrix * rest_vert * weight;
				}
			}
			result.push_back(mesh_transform * real_pos);
		}
		return result;
	}

	void DisplayPinchMats(DepthMap const& depth_map, Option<cv::Point3f> const& pinch_point) {
#ifdef _DEBUG
		IplImage *writeTo = cvCreateImage(cvSize(depth_map.w, depth_map.h), IPL_DEPTH_8U, 3);
		IplImage background = depth_map.normalized;
		cvMerge(&background, &background ,&background, NULL, writeTo);
		for (size_t i = 0; i < depth_map.binary.total(); i++) {
			writeTo->imageData[i * 3 + 2] = depth_map.binary.at<uchar>(i) > 0 ? 0xff : 0;
		}
		if (pinch_point) {
			cv::Point img_pt((*pinch_point).x * depth_map.w, (*pinch_point).y * depth_map.h);
			cvCircle(writeTo, img_pt, 3, CV_RGB(255, 0, 60), -1);
		}
		cvShowImage("result", writeTo);
		cvWaitKey(1);
		cvReleaseImage(&writeTo);
#endif
	}

	void ReportPxcBadStatus(const pxcStatus& status) {
		switch (status) {
		case PXC_STATUS_NO_ERROR:
			std::cout << "PXC_STATUS_NO_ERROR: Indicates the operation succeeded without any warning" << std::endl;
			break;
		case PXC_STATUS_FEATURE_UNSUPPORTED:
			std::cout << "PXC_STATUS_FEATURE_UNSUPPORTED Unsupported feature" << std::endl;
			break;
		case PXC_STATUS_PARAM_UNSUPPORTED:
			std::cout << "PXC_STATUS_PARAM_UNSUPPORTED Unsupported parameter(s)" << std::endl;
			break;
		case PXC_STATUS_ITEM_UNAVAILABLE:
			std::cout << "PXC_STATUS_ITEM_UNAVAILABLE Item not found/not available" << std::endl;
			break;

		case PXC_STATUS_HANDLE_INVALID:
			std::cout << "PXC_STATUS_HANDLE_INVALID Invalid session, algorithm instance, or pointer" << std::endl;
			break;
		case PXC_STATUS_ALLOC_FAILED:
			std::cout << "PXC_STATUS_ALLOC_FAILED Memory allocation failure" << std::endl;
			break;

		case PXC_STATUS_DEVICE_FAILED:
			std::cout << "PXC_STATUS_DEVICE_FAILED device failed due to malfunctioning" << std::endl;
			break;
		case PXC_STATUS_DEVICE_LOST:
			std::cout << "PXC_STATUS_DEVICE_LOST device failed due to unplug or unavailability" << std::endl;
			break;
		case PXC_STATUS_DEVICE_BUSY:
			std::cout << "PXC_STATUS_DEVICE_BUSY device busy" << std::endl;
			break;

		case PXC_STATUS_EXEC_ABORTED:
			std::cout << "PXC_STATUS_EXEC_ABORTED Execution aborted due to errors in upstream components" << std::endl;
			break;
		case PXC_STATUS_EXEC_INPROGRESS:
			std::cout << "PXC_STATUS_EXEC_INPROGRESS Asynchronous operation is in execution" << std::endl;
			break;
		case PXC_STATUS_EXEC_TIMEOUT:
			std::cout << "PXC_STATUS_EXEC_TIMEOUT Operation time out" << std::endl;
			break;

		case PXC_STATUS_FILE_WRITE_FAILED:
			std::cout << "PXC_STATUS_FILE_WRITE_FAILED Failure in open file in WRITE mode" << std::endl;
			break;
		case PXC_STATUS_FILE_READ_FAILED:
			std::cout << "PXC_STATUS_FILE_READ_FAILED Failure in open file in READ mode" << std::endl;
			break;
		case PXC_STATUS_FILE_CLOSE_FAILED:
			std::cout << "PXC_STATUS_FILE_CLOSE_FAILED Failure in close a file handle" << std::endl;
			break;

		case PXC_STATUS_DATA_UNAVAILABLE:
			std::cout << "PXC_STATUS_DATA_UNAVAILABLE Data not available for MW model or processing" << std::endl;
			break;

		case PXC_STATUS_DATA_NOT_INITIALIZED:
			std::cout << "PXC_STATUS_DATA_NOT_INITIALIZED Data failed to initialize" << std::endl;
			break;
		case PXC_STATUS_INIT_FAILED:
			std::cout << "PXC_STATUS_INIT_FAILED Module failure during initialization" << std::endl;
			break;

		case PXC_STATUS_STREAM_CONFIG_CHANGED:
			std::cout << "PXC_STATUS_STREAM_CONFIG_CHANGED Configuration for the stream has changed" << std::endl;
			break;

		case PXC_STATUS_POWER_UID_ALREADY_REGISTERED:
			std::cout << "PXC_STATUS_POWER_UID_ALREADY_REGISTERED" << std::endl;
			break;
		case PXC_STATUS_POWER_UID_NOT_REGISTERED:
			std::cout << "PXC_STATUS_POWER_UID_NOT_REGISTERED" << std::endl;
			break;
		case PXC_STATUS_POWER_ILLEGAL_STATE:
			std::cout << "PXC_STATUS_POWER_ILLEGAL_STATE" << std::endl;
			break;
		case PXC_STATUS_POWER_PROVIDER_NOT_EXISTS:
			std::cout << "PXC_STATUS_POWER_PROVIDER_NOT_EXISTS" << std::endl;
			break;

		case PXC_STATUS_CAPTURE_CONFIG_ALREADY_SET:
			std::cout << "PXC_STATUS_CAPTURE_CONFIG_ALREADY_SET parameter cannot be changed since configuration for capturing has been already set" << std::endl;
			break;
		case PXC_STATUS_COORDINATE_SYSTEM_CONFLICT:
			std::cout << "PXC_STATUS_COORDINATE_SYSTEM_CONFLICT  Mismatched coordinate system between modules" << std::endl;
			break;

			/* warnings */
		case PXC_STATUS_TIME_GAP:
			std::cout << "PXC_STATUS_TIME_GAP [WARNING] time gap in time stamps" << std::endl;
			break;
		case PXC_STATUS_PARAM_INPLACE:
			std::cout << "PXC_STATUS_PARAM_INPLACE [WARNING] the same parameters already defined" << std::endl;
			break;
		case PXC_STATUS_DATA_NOT_CHANGED:
			std::cout << "PXC_STATUS_DATA_NOT_CHANGED [WARNING] Data not changed (no new data available)" << std::endl;
			break;
		case PXC_STATUS_PROCESS_FAILED:
			std::cout << "PXC_STATUS_PROCESS_FAILED [WARNING] Module failure during processing" << std::endl;
			break;
		case PXC_STATUS_VALUE_OUT_OF_RANGE:
			std::cout << "PXC_STATUS_VALUE_OUT_OF_RANGE [WARNING] Data value(s) out of range" << std::endl;
			break;
		}
	}

	std::ostream& operator<<(std::ostream& os, Polycode::Matrix4 const& mat) {
		for (size_t i = 0; i < 16; i++)
		{
			os << mat.ml[i];
			if (i % 4 == 3)
				os << std::endl;
			else
				os << " ";
		}
		return os;
	}

	std::ostream& operator<<(std::ostream& os, Polycode::Vector2 const& vec) {
		os << "(" << vec.x << ", " << vec.y << ")";
		return os;
	}

	std::ostream& operator<<(std::ostream& os, Polycode::Vector3 const& vec) {
		os << "(" << vec.x << ", " << vec.y << ", " << vec.z << ")";
		return os;
	}

	std::ostream& operator<<(std::ostream& os, Polycode::Quaternion const& quat) {
		os << "[" << quat.w << ", " << quat.x << ", " << quat.y << ", " << quat.z << "]";
		return os;
	}

	Polycode::Vector2 CameraPointToScreen(Number x, Number y) {
		return Polycode::Vector2((x - 0.3) * 5 / 2 * kWinWidth, y * kWinHeight);
	}

}