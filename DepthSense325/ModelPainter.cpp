#include "ModelPainter.h"

#include <ctime>
#include <opencv2\opencv.hpp>

#include "Context.h"
#include "PenPicker.h"
#include "Util.h"

namespace mobamas {

ModelPainter::ModelPainter(std::shared_ptr<Context> context, Polycode::Scene *scene, Polycode::SceneMesh *mesh) :
	context_(context),
	EventHandler(),
	scene_(scene),
	mesh_(mesh),
	prev_tc_(),
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
	int first_vertex_index;
	double s, t; // intersection point is s(v1-v0) + t(v2-v0)
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
	auto a = - normal.dot(w0);
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
	res.s = s;
	res.t = t;
	return res;
}

static Intersection FindIntersectionPolygon(Polycode::SceneMesh *mesh, const Polycode::Ray& ray) {
	auto raw = mesh->getMesh();
	Intersection best;
	best.found = false;
	double distance = 1e10;

	std::vector<Polycode::Vector3> adjusted_points = ActualVertexPositions(mesh);

	int step = raw->getIndexGroupSize();
	for (int ii = 0; ii < raw->getIndexCount(); ii += step) {
		auto idx0 = raw->indexArray.data[ii],
			idx1 = raw->indexArray.data[ii+1],
			idx2 = raw->indexArray.data[ii+2];
		auto res = CalculateIntersectionPoint(ray, adjusted_points[idx0], adjusted_points[idx1], adjusted_points[idx2]);
		if (res.found) {
			res.first_vertex_index = ii;
			auto this_dist = res.point.distance(ray.origin);
			if (distance > this_dist) {
				best = res;
				distance = this_dist;
			}
		}
	}
	return best;
}

typedef std::pair<int ,int> TP;
static int Interpolate(const TP& start, const TP& end, int y) {
	double ratio = (y - start.second) / static_cast<double>(end.second - start.second);
	return start.first * (1 - ratio) + end.first * ratio;
}

const int kStampSize = 32;
static Polycode::Vector2 PaintTexture(Polycode::SceneMesh *mesh, Polycode::Vector2* prev_tc, const Intersection& intersection, const std::unique_ptr<PenPicker>& picker) {
	auto raw = mesh->getMesh();
	auto idx0 = intersection.first_vertex_index;
	auto tc0 = raw->getVertexTexCoordAtIndex(idx0);
	auto tc1 = raw->getVertexTexCoordAtIndex(idx0+1);
	auto tc2 = raw->getVertexTexCoordAtIndex(idx0+2);
	auto v0 = raw->getVertexPositionAtIndex(idx0);
	auto v1 = raw->getVertexPositionAtIndex(idx0+1);
	auto v2 = raw->getVertexPositionAtIndex(idx0+2);
	auto tc = (tc1 - tc0) * intersection.s + (tc2 - tc0) * intersection.t + tc0;

	auto texture = mesh->getTexture();
	auto height = texture->getHeight(), width = texture->getWidth();
	auto buffer = texture->getTextureData();
	cv::Mat tex_mat(height, width, CV_8UC4, buffer);

	auto TexPoint = [width, height](Polycode::Vector2 tc) {
		return cv::Point(static_cast<int>(tc.x * width), static_cast<int>(tc.y * height));
	};

	switch (picker->current_brush()) {
	case Brush::PEN:
		if (prev_tc == nullptr)
			return tc;
		{
			auto carea = (tc1 - tc0).crossProduct(tc2 - tc0);
			auto varea = (v1 - v0).crossProduct(v2 - v0).length();
			int normalized_pen_size = std::max(round(500 * picker->current_size() * carea / varea), 1.0);

			unsigned int rgba = picker->current_color().getUint();
			auto start = TexPoint(*prev_tc);
			auto end = TexPoint(tc);
			if (sqrt((end - start).dot(end - start)) < normalized_pen_size * 5) {
				cv::line(tex_mat, start, end, cv::Scalar(rgba & 0xff, (rgba >> 8) & 0xff, (rgba >> 16) & 0xff, 0xff), normalized_pen_size);
			}
		}
		break;
	case Brush::STAMP:
		if (prev_tc == nullptr || cv::norm(TexPoint(tc) - TexPoint(*prev_tc)) > kStampSize) {
			auto center = cv::Point(static_cast<int>(tc.x * width), static_cast<int>(tc.y * height));
			auto left_top = center - cv::Point(kStampSize / 2, kStampSize / 2);
			cv::Mat stamp_mat(kStampSize, kStampSize, CV_8UC4, picker->current_stamp()->getTextureData());
			cv::Rect dest_rect(
				std::max(left_top.x, 0),
				std::max(left_top.y, 0),
				std::min({ kStampSize, kStampSize + left_top.x, width - left_top.x }),
				std::min({ kStampSize, kStampSize + left_top.y, height - left_top.y }));
			cv::Mat dest_roi(tex_mat, dest_rect);
			cv::Mat src_roi(stamp_mat, cv::Rect(
				std::max(-left_top.x, 0),
				std::max(-left_top.y, 0),
				dest_rect.width,
				dest_rect.height));
			std::vector<cv::Mat> channels;
			cv::split(src_roi, channels);
			src_roi.copyTo(dest_roi, channels[3]);
		}
		else {
			return *prev_tc;
		}
		break;
	}
#ifdef _DEBUG
	cv::imshow("win", tex_mat);
#endif
	texture->recreateFromImageData();
	return tc;
}

const int kMouseLeftButtonCode = 0;

void ModelPainter::handleEvent(Polycode::Event *e) {
	using Polycode::InputEvent;
	auto paint = [&]() {
		InputEvent *ie = (InputEvent*)e;
		auto ray = scene_->projectRayFromCameraAndViewportCoordinate(scene_->getActiveCamera(), ie->getMousePosition());
		auto raw = mesh_->getMesh();
		assert(raw->getMeshType() == Polycode::Mesh::TRI_MESH);
		auto intersection = FindIntersectionPolygon(mesh_, ray);
		if (intersection.found) {
			prev_tc_.reset(new Polycode::Vector2(PaintTexture(mesh_, prev_tc_.get(), intersection, picker_)));
		}
	};

	auto set_click_state = [&](bool new_val) {
		InputEvent *ie = (InputEvent*)e;
		if (ie->mouseButton == kMouseLeftButtonCode) {
			left_clicking_ = new_val;
			context_->logfs << time(nullptr) << ": Paint" << (new_val ? "Start" : "End") << std::endl;
		}
		prev_tc_.release();
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