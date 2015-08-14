#include "ModelPainter.h"

#include <ctime>
#include <unordered_set>
#include <opencv2\opencv.hpp>

#include "Context.h"
#include "EditorApp.h"
#include "Import.h"
#include "PenPicker.h"
#include "Util.h"

namespace mobamas {

const int kMouseLeftButtonCode = 0;
const float kDisplayCanvasRatio = 1; // set < 1 to reduce canvas size
const int kStampSize = 32; // on display

	ModelPainter::ModelPainter(std::shared_ptr<Context> context, Polycode::Scene *scene, MeshGroup *mesh) :
		context_(context),
		EventHandler(),
		scene_(scene),
		mesh_(mesh),
		prev_mouse_pos_(),
		canvas_(kWinHeight * kDisplayCanvasRatio, kWinWidth * kDisplayCanvasRatio, CV_8UC4),
		left_clicking_(false)
	{
		picker_.reset(new PenPicker(context));

		auto input = Polycode::CoreServices::getInstance()->getInput();
		using Polycode::InputEvent;
		input->addEventListener(this, InputEvent::EVENT_MOUSEMOVE);
		input->addEventListener(this, InputEvent::EVENT_MOUSEDOWN);
		input->addEventListener(this, InputEvent::EVENT_MOUSEUP);
	}

	// http://geomalgorithms.com/a06-_intersect-2.html#intersect3D_RayTriangle()
	struct Intersection {
		bool found;
		Polycode::SceneMesh *scene_mesh;
		int first_vertex_index;
		Polycode::Vector3 point;
	};

	const double kEpsilon = 0.000001;

	static Intersection CalculateIntersectionPoint(const Polycode::Ray& ray,
		const Polycode::Vector3& v0,
		const Polycode::Vector3& v1,
		const Polycode::Vector3& v2)
	{
		Intersection res;
		res.found = false;
		auto u = v1 - v0, v = v2 - v0;
		auto normal = u.crossProduct(v);
		if (normal.length() < kEpsilon) {
			return res;
		}
		auto w0 = ray.origin - v0;
		auto a = -normal.dot(w0);
		auto b = normal.dot(ray.direction);
		if (fabs(b) < kEpsilon) {
			return res;
		}
		auto r = a / b;
		if (r < 0.0) {
			return res;
		}
		auto intersection = ray.origin + ray.direction * r;

		auto uu = u.dot(u), uv = u.dot(v), vv = v.dot(v);
		auto w = intersection - v0;
		auto wu = w.dot(u), wv = w.dot(v);
		auto d = uv * uv - uu * vv;

		auto s = (uv * wv - vv * wu) / d;
		if (s < 0.0 || s > 1.0)
			return res;
		auto t = (uv * wu - uu * wv) / d;
		if (t < 0.0 || (s + t) > 1.0)
			return res;
		res.found = true;
		res.point = intersection;
		return res;
	}

	static Intersection FindIntersectionPolygon(vector<Polycode::SceneMesh*> const& meshes, const Polycode::Ray& ray) {
		Intersection best;
		best.found = false;
		double distance = 1e10;

		for (auto mesh : meshes) {
			auto raw = mesh->getMesh();
			std::vector<Polycode::Vector3> adjusted_points = ActualVertexPositions(mesh);
			int step = raw->getIndexGroupSize();
			for (int ii = 0; ii < raw->getIndexCount(); ii += step) {
				auto idx0 = raw->indexArray.data[ii],
					idx1 = raw->indexArray.data[ii + 1],
					idx2 = raw->indexArray.data[ii + 2];
				auto res = CalculateIntersectionPoint(ray, adjusted_points[idx0], adjusted_points[idx1], adjusted_points[idx2]);
				if (res.found) {
					res.first_vertex_index = ii;
					res.scene_mesh = mesh;
					auto this_dist = res.point.distance(ray.origin);
					if (distance > this_dist) {
						best = std::move(res);
						distance = this_dist;
					}
				}
			}
		}
		return best;
	}

	typedef std::pair<int, int> TP;
	static int Interpolate(const TP& start, const TP& end, int y) {
		double ratio = (y - start.second) / static_cast<double>(end.second - start.second);
		return start.first * (1 - ratio) + end.first * ratio;
	}

void ModelPainter::PrepareCanvas(Polycode::Vector2 const& mouse_pos) {
	bool update_pos = false;
	canvas_ = cv::Scalar(0, 0, 0, 0);
	switch (picker_->current_brush()) {
	case Brush::PEN:
	{
		int cv_pen_size = PenPicker::DisplaySize(picker_->current_size()) * kDisplayCanvasRatio / 2;
		if (prev_mouse_pos_) {
			cv::line(canvas_, ToCv(*prev_mouse_pos_, kDisplayCanvasRatio), ToCv(mouse_pos, kDisplayCanvasRatio),
				ToCv(picker_->current_color()), cv_pen_size);
		}
		else {
			cv::circle(canvas_, ToCv(mouse_pos, kDisplayCanvasRatio), cv_pen_size, ToCv(picker_->current_color()), -1);
		}
		update_pos = true;
	}
		break;
	case Brush::STAMP:
		if (prev_mouse_pos_ == nullptr || mouse_pos.distance(*prev_mouse_pos_) > kStampSize) {
			update_pos = true;
			auto left_top = ToCv(mouse_pos) - cv::Point(kStampSize / 2, kStampSize / 2);
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
			cv::Mat dest_roi(canvas_, dest_rect);
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
		}
		break;
	}
	if (update_pos) {
		if (prev_mouse_pos_) {
			*prev_mouse_pos_ = mouse_pos;
		} else {
			prev_mouse_pos_.reset(new Polycode::Vector2(mouse_pos));
		}
	}
}

static inline bool HasNonZero(cv::Mat const& mat) {
	auto tc = mat.channels();
	for (int y = 0; y < mat.rows; ++y) {
		const uchar* ptr = mat.ptr<uchar>(y);
		for (int x = 0; x < mat.cols; ++x) {
			bool non_zero = false;
			for (int c = 0; c < tc; ++c) {
				non_zero = non_zero || ptr[x * tc + c] != 0;
			}
			if (non_zero)
				return true;
		}
	}
	return false;
}

static void overlay(cv::Mat& texture, cv::Mat const& new_paint) {
	assert(texture.size() == new_paint.size());
	assert(new_paint.channels() == 4);
	auto tc = texture.channels();
	for (int y = 0; y < texture.rows; ++y) {
		uchar* tp = texture.ptr<uchar>(y);
		const uchar* np = new_paint.ptr<uchar>(y);
		for (int x = 0; x < texture.cols; ++x) {
			double opacity = ((double)np[x * 4 + 3]) / 255.;
			if (opacity < 0.000001)
				continue;
			for (int c = 0; c < 3; ++c) {
				// swap channels because OpenCV is bgra, whereas polycode is rgba
				tp[x * tc + c] = tp[x * tc + c] * (1. - opacity) + np[x * 4 + (2 - c)] * opacity;
			}
		}
	}
}

void ModelPainter::PaintTexture(Intersection const& intersection) {
	auto mesh = intersection.scene_mesh;
	auto raw = mesh->getMesh();
	assert(raw->getMeshType() == Polycode::Mesh::TRI_MESH);
	auto texture = mesh->getTexture();
	auto height = texture->getHeight(), width = texture->getWidth();
	auto buffer = texture->getTextureData();
	cv::Mat tex_mat(height, width, CV_8UC4, buffer);
	cv::Mat new_paint(height, width, CV_8UC4);

	auto vertex_positions = ActualVertexPositions(mesh);
	auto renderer = Polycode::CoreServices::getInstance()->getRenderer();
	renderer->BeginRender();
	renderer->setPerspectiveDefaults();
	scene_->getActiveCamera()->doCameraTransform();
	auto camera = renderer->getCameraMatrix();
	auto projection = renderer->getProjectionMatrix();
	auto view = renderer->getViewport();

	std::unordered_set<unsigned int> visited;
	std::deque<unsigned int> waiting;
	bool updated = false;
	waiting.push_front(intersection.first_vertex_index);

	while (!waiting.empty()) {
		auto idx = waiting.front();
		waiting.pop_front();
		visited.insert(idx);

		cv::Point2f screen[3];
		float left, top, right, bottom;
		for (size_t i = 0; i < 3; i++) {
			auto v = vertex_positions[raw->indexArray.data[idx + i]];
			screen[i] = ToCv(renderer->Project(camera, projection, view, v));
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
		std::cout << left << ", " << top << ", " << right << "," << bottom << std::endl;
		cv::Mat mask(std::ceil(bottom - top), std::ceil(right - left), CV_8UC1, cv::Scalar(0));
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
		cv::Rect inter = mask_on_canvas & cv::Rect(cv::Point(0, 0), canvas_.size());
		cv::Mat mask_roi = mask(inter + cv::Point(std::min(static_cast<int>(std::floor(-left)), 0), std::min(static_cast<int>(std::floor(-top)), 0)));
		cv::Mat overlap;
		canvas_(inter).copyTo(overlap, mask_roi);
		if (!HasNonZero(overlap))
			continue;

		cv::Point2f tex[3];
		for (size_t i = 0; i < 3; i++) {
			auto uv = raw->getVertexTexCoordAtIndex(idx + i);
			tex[i] = cv::Point2f(uv.x * width, uv.y * height);
		}
		auto trans = cv::getAffineTransform(zero_screen, tex);
		cv::warpAffine(overlap, new_paint, trans, new_paint.size());
		overlay(tex_mat, new_paint);
		updated = true;

		// optimize: start searcing from current index, and stop if 3 hits found
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
	if (updated)
		texture->recreateFromImageData();
}

void ModelPainter::handleEvent(Polycode::Event *e) {
	using Polycode::InputEvent;
	auto paint = [&]() {
		InputEvent *ie = (InputEvent*)e;
		auto mouse_pos = ie->getMousePosition();
		PrepareCanvas(mouse_pos);

		auto ray = scene_->projectRayFromCameraAndViewportCoordinate(scene_->getActiveCamera(), mouse_pos);
		auto intersection = FindIntersectionPolygon(mesh_->getSceneMeshes(), ray);
		if (intersection.found) {
			PaintTexture(intersection);
		}
	};

	auto set_click_state = [&](bool new_val) {
		InputEvent *ie = (InputEvent*)e;
		if (ie->mouseButton == kMouseLeftButtonCode) {
			left_clicking_ = new_val;
			context_->logfs << time(nullptr) << ": Paint" << (new_val ? "Start" : "End") << std::endl;
		}
		prev_mouse_pos_.release();
	};

	switch (e->getEventCode()) {
	case InputEvent::EVENT_MOUSEMOVE:
		if (left_clicking_)
			paint();
		break;
	case InputEvent::EVENT_MOUSEDOWN:
		set_click_state(true);
		paint();
		break;
	case InputEvent::EVENT_MOUSEUP:
		set_click_state(false);
		break;
	}
}

}