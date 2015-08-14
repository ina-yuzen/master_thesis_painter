#include "ModelPainter.h"

#include <chrono>
#include <ctime>
#include <unordered_set>
#include <opencv2\opencv.hpp>
#include <opencv2\ocl\ocl.hpp>

#include "Context.h"
#include "EditorApp.h"
#include "Import.h"
#include "Intersection.h"
#include "PenPicker.h"
#include "Util.h"

namespace mobamas {

const int kMouseLeftButtonCode = 0;
const float kDisplayCanvasRatio = 1; // set < 1 to reduce canvas size
const int kStampSize = 32; // on display
const int kPenMoveThreshold = 5;
const Polycode::Vector2 kInvalidPoint(-1, -1);

class PaintWorker {
public:
	PaintWorker(std::shared_ptr<Context> context, Polycode::Scene *scene, MeshGroup *mesh);
	void UpdateNextPoint(Polycode::Vector2 const& p);
	void FinishPainting() { last_pos_ = kInvalidPoint; }
	void WorkOff();
	void UpdateOnMain();

private:
	std::shared_ptr<Context> context_;
	Polycode::Scene* scene_;
	MeshGroup* mesh_;
	Polycode::Vector2 last_pos_;
	std::unique_ptr<PenPicker> picker_;
	std::unique_ptr<cv::Mat> front_canvas_, back_canvas_;
	std::atomic_bool front_dirty_;
	Polycode::Matrix4 camera_, projection_;
	Polycode::Rectangle view_;
	std::vector<Polycode::Texture*> dirty_textures_;

	bool PaintCanvas(Polycode::Vector2 const& last, Polycode::Vector2 const& next);
	void PaintTexture(Intersection const& intersection);
};

void WorkerThreadMainLoop(std::atomic_bool* interrupted, PaintWorker* worker) {
	std::chrono::milliseconds loop(33);
	using std::chrono::system_clock;
	system_clock::time_point start;
	while (!interrupted->load()) {
		start = system_clock::now();
		worker->WorkOff();
		auto end = system_clock::now();
		if (end - start < loop) {
			Sleep(std::chrono::duration_cast<std::chrono::milliseconds>(loop - (end - start)).count());
		}
	}
}

ModelPainter::ModelPainter(std::shared_ptr<Context> context, Polycode::Scene *scene, MeshGroup *mesh) :
    EventHandler(),
    context_(context),
    worker_(new PaintWorker(context, scene, mesh)),
	inturrupted_(),
	left_clicking_(false) {

	inturrupted_.store(false);
	worker_thread_ = std::move(std::thread(WorkerThreadMainLoop, &inturrupted_, worker_.get()));

	auto input = Polycode::CoreServices::getInstance()->getInput();
	using Polycode::InputEvent;
	input->addEventListener(this, InputEvent::EVENT_MOUSEMOVE);
	input->addEventListener(this, InputEvent::EVENT_MOUSEDOWN);
	input->addEventListener(this, InputEvent::EVENT_MOUSEUP);
}

void ModelPainter::handleEvent(Polycode::Event *e) {
	using Polycode::InputEvent;
	InputEvent *ie = (InputEvent*)e;

	auto set_click_state = [&](bool new_val) {
		if (ie->mouseButton == kMouseLeftButtonCode) {
			left_clicking_ = new_val;
			context_->logfs << time(nullptr) << ": Paint" << (new_val ? "Start" : "End") << std::endl;
		}
	};

	switch (e->getEventCode()) {
	case InputEvent::EVENT_MOUSEMOVE:
		if (left_clicking_) {
			worker_->UpdateNextPoint(ie->getMousePosition());
		}
		break;
	case InputEvent::EVENT_MOUSEDOWN:
		set_click_state(true);
		worker_->UpdateNextPoint(ie->getMousePosition());
		break;
	case InputEvent::EVENT_MOUSEUP:
		set_click_state(false);
		worker_->FinishPainting();
		break;
	}
}

void ModelPainter::Update() {
	worker_->UpdateOnMain();
}

void ModelPainter::Shutdown() {
	inturrupted_.store(true);
	worker_thread_.join();
}

PaintWorker::PaintWorker(std::shared_ptr<Context> context, Polycode::Scene *scene, MeshGroup *mesh) : 
	context_(context),
	scene_(scene),
	mesh_(mesh),
	last_pos_(kInvalidPoint),
	front_canvas_(new cv::Mat(kWinHeight * kDisplayCanvasRatio, kWinWidth * kDisplayCanvasRatio, CV_8UC4)),
	back_canvas_(new cv::Mat(kWinHeight * kDisplayCanvasRatio, kWinWidth * kDisplayCanvasRatio, CV_8UC4)) {

	picker_.reset(new PenPicker(context));
}

inline bool operator ==(const Polycode::Vector2 &a, const Polycode::Vector2 &b) {
	return a.x == b.x && a.y == b.y;
}

bool PaintWorker::PaintCanvas(Polycode::Vector2 const& last, Polycode::Vector2 const& next) {
	switch (picker_->current_brush()) {
	case Brush::PEN:
	{
		int cv_pen_size = PenPicker::DisplaySize(picker_->current_size()) * kDisplayCanvasRatio / 2;
		if (last == kInvalidPoint) {
			cv::circle(*front_canvas_, ToCv<cv::Point>(next, kDisplayCanvasRatio), cv_pen_size, ToCv(picker_->current_color()), -1);
			return true;
		} else if (next.distance(last) > kPenMoveThreshold) {
			cv::line(*front_canvas_, ToCv<cv::Point>(last, kDisplayCanvasRatio), ToCv<cv::Point>(next, kDisplayCanvasRatio),
				ToCv(picker_->current_color()), cv_pen_size);
			return true;
		}
	}
		break;
	case Brush::STAMP:
		if (last == kInvalidPoint || next.distance(last) > kStampSize) {
			auto left_top = ToCv<cv::Point>(next) - cv::Point(kStampSize / 2, kStampSize / 2);
			cv::Mat stamp_mat(kStampSize, kStampSize, CV_8UC4);
			auto tex = picker_->current_stamp()->getTextureData();
			// correct y-reverse and rgba -> bgra
			for (size_t y = 0; y < kStampSize; y++) {
				auto ptr = stamp_mat.ptr<uchar>(y);
				auto inv_y = kStampSize - y - 1;
				for (size_t x = 0; x < kStampSize; x++) {
					for (size_t i = 0; i < 4; i++) {
						ptr[x * 4 + i] = tex[(inv_y * kStampSize + x) * 4 + (i < 3 ? (2 - i) : i)];
					}
				}
			}
			cv::Rect dest_rect(
				kDisplayCanvasRatio * std::max(left_top.x, 0),
				kDisplayCanvasRatio * std::max(left_top.y, 0),
				kDisplayCanvasRatio * std::min({ kStampSize, kStampSize + left_top.x, kWinWidth - left_top.x }),
				kDisplayCanvasRatio * std::min({ kStampSize, kStampSize + left_top.y, kWinHeight - left_top.y }));
			cv::Mat dest_roi(*front_canvas_, dest_rect);
			if (kDisplayCanvasRatio != 1) {
				cv::Mat resized(kStampSize * kDisplayCanvasRatio, kStampSize * kDisplayCanvasRatio, CV_8UC4);
				cv::resize(stamp_mat, resized, resized.size());
				stamp_mat = resized;
			}
			cv::Mat src_roi(stamp_mat, cv::Rect(
				kDisplayCanvasRatio * std::max(-left_top.x, 0),
				kDisplayCanvasRatio * std::max(-left_top.y, 0),
				dest_rect.width,
				dest_rect.height));
			std::vector<cv::Mat> channels;
			cv::split(src_roi, channels);
			src_roi.copyTo(dest_roi, channels[3]);
			return true;
		}
		break;
	}
	return false;
}

static inline bool HasNonZero(cv::Mat const& mat) {
	auto tc = mat.channels();
	for (int y = 0; y < mat.rows; ++y) {
		const uchar* ptr = mat.ptr<uchar>(y);
		for (int x = 0; x < mat.cols; ++x) {
			for (int c = 0; c < tc; ++c) {
				if (ptr[x * tc + c] != 0)
					return true;
			}
		}
	}
	return false;
}

// FIXME: GPU is appropriate for this task
static void overlay(cv::Mat& texture, cv::Mat const& new_paint) {
	assert(texture.size() == new_paint.size());
	assert(new_paint.channels() == 4);
	auto tc = texture.channels();
	for (int y = 0; y < texture.rows; ++y) {
		uchar* tp = texture.ptr<uchar>(y);
		const uchar* np = new_paint.ptr<uchar>(y);
		for (int x = 0; x < texture.cols; ++x) {
			int alpha = np[x * 4 + 3];
			for (int c = 0; c < 3; ++c) {
				// swap channels because OpenCV is bgra, whereas polycode is rgba
				tp[x * tc + c] = (tp[x * tc + c] * (256 - alpha) + np[x * 4 + (2 - c)] * alpha) >> 8;
			}
		}
	}
}

void PaintWorker::PaintTexture(Intersection const& intersection) {
	auto mesh = intersection.scene_mesh;
	auto raw = mesh->getMesh();
	assert(raw->getMeshType() == Polycode::Mesh::TRI_MESH);
	auto texture = mesh->getTexture();
	auto height = texture->getHeight(), width = texture->getWidth();
	auto buffer = texture->getTextureData();
	cv::Mat tex_mat(height, width, CV_8UC4, buffer);
	cv::ocl::oclMat new_paint(height, width, CV_8UC4);

	auto vertex_positions = ActualVertexPositions(mesh);

	std::unordered_set<unsigned int> visited;
	std::deque<unsigned int> waiting;
	bool updated = false;
	waiting.push_front(intersection.first_vertex_index);
	auto renderer = Polycode::CoreServices::getInstance()->getRenderer();

	while (!waiting.empty()) {
		auto idx = waiting.front();
		waiting.pop_front();
		visited.insert(idx);

		cv::Point2f screen[3];
		float left, top, right, bottom;
		for (size_t i = 0; i < 3; i++) {
			auto v = vertex_positions[raw->indexArray.data[idx + i]];
			screen[i] = ToCv<cv::Point2f>(renderer->Project(camera_, projection_, view_, v));
			if (i == 0) {
				left = right = screen[i].x;
				top = bottom = screen[i].y;
			} else {
				if (screen[i].x < left)
					left = screen[i].x;
				if (screen[i].x > right)
					right = screen[i].x;
				if (screen[i].y < top)
					top = screen[i].y;
				if (screen[i].y > bottom)
					bottom = screen[i].y;
			}
		}
		cv::Mat mask(bottom - top + 1, right - left + 1, CV_8UC1, cv::Scalar(0));
		cv::Point pts[3];
		cv::Point2f zero_screen[3];
		for (size_t i = 0; i < 3; i++) {
			zero_screen[i] = cv::Point2f(screen[i].x - left, screen[i].y - top);
			pts[i] = zero_screen[i];
		}
		const cv::Point *arr[1] = { pts };
		int npts[] = { 3 };
		cv::fillPoly(mask, arr, npts, 1, cv::Scalar(255));
		cv::Rect mask_on_canvas(left, top, mask.cols, mask.rows);
		cv::Rect inter = mask_on_canvas & cv::Rect(cv::Point(0, 0), back_canvas_->size());
		cv::Mat mask_roi = mask((inter - cv::Point(left, top)) & cv::Rect(cv::Point(0, 0), mask.size()));
		cv::Mat overlap;
		(*back_canvas_)(inter).copyTo(overlap, mask_roi);
		if (!HasNonZero(overlap))
			continue;

		cv::ocl::oclMat g_overlap(overlap);
		cv::Point2f tex[3];
		for (size_t i = 0; i < 3; i++) {
			auto uv = raw->getVertexTexCoordAtIndex(idx + i);
			tex[i] = cv::Point2f(uv.x * width, uv.y * height);
		}
		auto trans = cv::getAffineTransform(zero_screen, tex);
		cv::ocl::warpAffine(g_overlap, new_paint, trans, new_paint.size());
		overlay(tex_mat, cv::Mat(new_paint));
		updated = true;

		// optimize: start searcing from current index, and stop if 3 hits found
		// FIXME: background faces are also selected (rarely occurs)
		auto ia = raw->indexArray.data;
		auto v1 = raw->getVertexPositionAtIndex(idx), 
			v2 = raw->getVertexPositionAtIndex(idx + 1), 
			v3 = raw->getVertexPositionAtIndex(idx + 2);
		for (size_t i = 0, size = raw->indexArray.getDataSize(); i < size; i += 3) {
			auto t1 = raw->getVertexPositionAtIndex(i),
				t2 = raw->getVertexPositionAtIndex(i + 1),
				t3 = raw->getVertexPositionAtIndex(i + 2);
			if (visited.find(i) == visited.end()
				&&(v1 == t3 && v2 == t2 || v1 == t2 && v2 == t3 
				|| v1 == t3 && v3 == t2 || v1 == t2 && v3 == t3
				|| v3 == t3 && v2 == t2 || v3 == t2 && v2 == t3
				|| v1 == t1 && v2 == t3 || v1 == t3 && v2 == t1 
				|| v1 == t1 && v3 == t3 || v1 == t3 && v3 == t1
				|| v3 == t1 && v2 == t3 || v3 == t3 && v2 == t1
				|| v1 == t1 && v2 == t2 || v1 == t2 && v2 == t1 
				|| v1 == t1 && v3 == t2 || v1 == t2 && v3 == t1
				|| v3 == t1 && v2 == t2 || v3 == t2 && v2 == t1)) {

				waiting.push_front(i);
			}
		}
	}
	if (updated) {
		dirty_textures_.push_back(texture);
	}
}

void PaintWorker::UpdateNextPoint(Polycode::Vector2 const& p) {
	if (!PaintCanvas(last_pos_, p))
		return;
	front_dirty_.store(true);
	last_pos_ = p;
}

void PaintWorker::WorkOff() {
	if (!front_dirty_.exchange(false)) {
		return;
	}
	std::swap(front_canvas_, back_canvas_);
	auto ray = scene_->projectRayFromCameraAndViewportCoordinate(scene_->getActiveCamera(), last_pos_);
	auto intersection = FindIntersectionPolygon(mesh_->getSceneMeshes(), ray);
	if (intersection.found) {
		PaintTexture(intersection);
	}
	*back_canvas_ = cv::Scalar(0, 0, 0, 0);
}

void PaintWorker::UpdateOnMain() {
	auto renderer = Polycode::CoreServices::getInstance()->getRenderer();
	renderer->BeginRender();
	renderer->setPerspectiveDefaults();
	scene_->getActiveCamera()->doCameraTransform();
	camera_ = renderer->getCameraMatrix();
	projection_ = renderer->getProjectionMatrix();
	view_ = renderer->getViewport();
	renderer->EndRender();
	for (auto t : dirty_textures_) {
		t->recreateFromImageData();
	}
	dirty_textures_.clear();
}

}