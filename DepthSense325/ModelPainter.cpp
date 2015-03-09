#include "ModelPainter.h"

#include <opencv2\opencv.hpp>

#include "ColorPicker.h"
#include "Util.h"

namespace mobamas {

ModelPainter::ModelPainter(Polycode::Scene *scene, Polycode::SceneMesh *mesh) :
	EventHandler(),
	scene_(scene),
	mesh_(mesh),
	prev_tc_(),
	left_clicking_(false)
{
	picker_.reset(new ColorPicker());

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

const double kPenSize = 500;

static Polycode::Vector2 FillTexture(Polycode::SceneMesh *mesh, Polycode::Vector2* prev_tc, const Intersection& intersection, const Polycode::Color& color) {
	auto raw = mesh->getMesh();
	auto idx0 = intersection.first_vertex_index;
	auto tc0 = raw->getVertexTexCoordAtIndex(idx0);
	auto tc1 = raw->getVertexTexCoordAtIndex(idx0+1);
	auto tc2 = raw->getVertexTexCoordAtIndex(idx0+2);
	auto v0 = raw->getVertexPositionAtIndex(idx0);
	auto v1 = raw->getVertexPositionAtIndex(idx0+1);
	auto v2 = raw->getVertexPositionAtIndex(idx0+2);
	auto tc = (tc1 - tc0) * intersection.s + (tc2 - tc0) * intersection.t + tc0;

	if (prev_tc == nullptr)
		return tc;

	auto carea = (tc1-tc0).crossProduct(tc2-tc0);
	auto varea = (v1-v0).crossProduct(v2-v0).length();
	int normalized_pen_size = std::max(round(kPenSize * carea / varea), 1);

	auto texture = mesh->getTexture();
	auto height = texture->getHeight(), width = texture->getWidth();

	auto buffer = texture->getTextureData();
	unsigned int rgba = color.getUint();
	auto start = cv::Point(static_cast<int>(prev_tc->x * width), static_cast<int>(prev_tc->y * height));
	auto end = cv::Point(static_cast<int>(tc.x * width), static_cast<int>(tc.y * height));
	if (sqrt((end - start).dot(end - start)) < normalized_pen_size * 5) {
		cv::Mat tex_mat(height, width, CV_8UC4, buffer);
		cv::line(tex_mat, start, end, cv::Scalar(rgba & 0xff, (rgba >> 8) & 0xff, (rgba >> 16) & 0xff, 0xff), normalized_pen_size);
		texture->recreateFromImageData();
	}
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
			prev_tc_.reset(new Polycode::Vector2(FillTexture(mesh_, prev_tc_.get(), intersection, picker_->current_color())));
		}
	};

	auto set_click_state = [&](bool new_val) {
		InputEvent *ie = (InputEvent*)e;
		if (ie->mouseButton == kMouseLeftButtonCode)
			left_clicking_ = new_val;
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