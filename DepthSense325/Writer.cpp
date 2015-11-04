#include "Writer.h"

#include <ctime>
#include <cstdio>
#include <chrono>
#include <codecvt>
#include <locale>

#include "Recorder.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace mobamas {

static std::wstring strftime(std::wstring const& format) {
	wchar_t buffer[1024];
	auto now = std::chrono::system_clock::now();
	auto tt = std::chrono::system_clock::to_time_t(now);
	struct tm* tm_info = localtime(&tt);
	wcsftime(buffer, 1024, format.c_str(), tm_info);
	return std::wstring(buffer);
}

static std::wstring ToString(Models model) {
	switch (model) {
	case ROBOT:
		return L"robot";
	case MIKU:
		return L"miku";
	case TREASURE:
		return L"treasure";
	case DOG:
		return L"dog";
	default:
		assert(false && "unspecified model case");
	}
}

static std::wstring ToString(OperationMode model) {
	switch (model) {
	case MouseMode:
		return L"mouse";
	case TouchMode:
		return L"touch";
	case MidAirMode:
		return L"midair";
	case FrontMode:
		return L"front";
	default:
		assert(false && "unspecified model case");
	}
}

Writer::Writer(Models const& model, OperationMode const& mode) : recorder_(nullptr) {
	dirname_ = strftime(L"rec_%m%d-%H%M") + L"_" 
		+ ToString(model) + L"_" 
		+ ToString(mode);
	assert(_wmkdir(dirname_.c_str()) == 0 && "Failed to create writer directory");
	log_.open(dirname_ + L"/log.txt", std::ios::out);
	assert(log_.is_open() && "Failed to open log file");
}

Writer::~Writer() {
	log_.close();
}

void Writer::WriteTexture(Polycode::Texture* texture) {
	if (!texture)
		return;
	auto height = texture->getHeight(), width = texture->getWidth();
	auto buffer = texture->getTextureData();

	// “ú–{Œê–¼‚ª‚¨‚©‚µ‚©‚Á‚½‚çŒŸ“¢
	auto orig_path = texture->getResourcePath().contents;
	auto name = orig_path.substr(orig_path.find_last_of("\\/") + 1);
	std::string dir_multi;
	wstrToUtf8(dir_multi, dirname_);
	auto path = dir_multi + "/" + name;
	auto ext = name.substr(name.find_last_of(".") + 1);
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

	if (ext == "png") {
		stbi_write_png(path.c_str(), width, height, 4, buffer, width * 4);
	} else if (ext == "tga") {
		stbi_write_tga(path.c_str(), width, height, 4, buffer);
	} else if (ext == "bmp") {
		stbi_write_bmp(path.c_str(), width, height, 4, buffer);
	} else if (ext == "jpg") {
		cv::Mat orig(height, width, CV_8UC4, buffer);
		cv::Mat mat;
		orig.copyTo(mat);
		for (size_t y = 0; y < height; y++) {
			auto ptr = mat.ptr<uchar>(y);
			for (size_t x = 0; x < width; x++) {
				uchar r = ptr[x * 4];
				ptr[x * 4] = ptr[x * 4 + 2];
				ptr[x * 4 + 2] = r;
			}
		}
		cv::imwrite(path, mat);
	} else {
		assert(false && "Unsupported export texture format");
	}
}

const std::locale utf8_locale = std::locale(std::locale(), new std::codecvt_utf8<wchar_t>());

void Writer::WritePose(Polycode::Skeleton* skeleton) {
	if (!skeleton)
		return;
	std::wofstream w(dirname_ + L"/pose.txt", std::ios::out);
	w.imbue(utf8_locale);
	for (size_t i = 0, length = skeleton->getNumBones(); i < length; i++) {
		auto bone = skeleton->getBone(i);
		auto mat = bone->getTransformMatrix();
		std::wstring name;
		utf8toWStr(name, bone->getName().contents);
		w << name;
		for (size_t i = 0; i < 16; i++) {
			w << " " << mat.ml[i];
		}
		w << std::endl;

	}
	w.close();
}

std::wostream& Writer::log() {
	log_ << strftime(L"%Y-%m-%d %H:%M:%S") << L"  ";
	return log_;
}

}
