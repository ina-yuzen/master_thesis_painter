#include "ModelPainter.h"
#include "ColorPicker.h"

namespace mobamas {

ModelPainter::ModelPainter(Polycode::Scene *scene, Polycode::SceneMesh *mesh) :
	EventHandler(),
	scene_(scene),
	mesh_(mesh),
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

	std::vector<Polycode::Vector3> adjusted_points;
	auto mesh_transform = mesh->getTransformMatrix();
	for (int vidx=0; vidx < raw->vertexPositionArray.data.size()/3; vidx ++) {
		auto rest_vert = Polycode::Vector3(raw->vertexPositionArray.data[vidx*3],
			raw->vertexPositionArray.data[vidx*3+1],
			raw->vertexPositionArray.data[vidx*3+2]);
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
		adjusted_points.push_back(mesh_transform * real_pos);
	}

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

static void FillTexture(Polycode::SceneMesh *mesh, const Intersection& intersection, const Polycode::Color& color) {
	auto raw = mesh->getMesh();
	auto idx0 = intersection.first_vertex_index;
	auto tc0 = raw->getVertexTexCoordAtIndex(idx0);
	auto tc1 = raw->getVertexTexCoordAtIndex(idx0+1);
	auto tc2 = raw->getVertexTexCoordAtIndex(idx0+2);
	auto v0 = raw->getVertexPositionAtIndex(idx0);
	auto v1 = raw->getVertexPositionAtIndex(idx0+1);
	auto v2 = raw->getVertexPositionAtIndex(idx0+2);
	auto tc = (tc1 - tc0) * intersection.s + (tc2 - tc0) * intersection.t + tc0;

	auto carea = (tc1-tc0).crossProduct(tc2-tc0);
	auto varea = (v1-v0).crossProduct(v2-v0).length();
	int normalized_pen_size = kPenSize * carea / varea;

	auto texture = mesh->getTexture();
	auto height = texture->getHeight(), width = texture->getWidth();

	auto buffer = texture->getTextureData();
	unsigned int rgba = color.getUint();
	auto cx = width * tc.x, cy = height * tc.y;
	for (int dy = -normalized_pen_size; dy <= normalized_pen_size; dy ++) {
		int y = cy + dy;
		if (y < 0 || y >= height)
			continue;
		for (int dx = - normalized_pen_size + abs(dy); dx <= normalized_pen_size - abs(dy); dx++) {
			int x = cx + dx;
			if (x < 0 || x >= width)
				continue;
			int i = y * width + x;
			buffer[i * 4    ] = rgba & 0xff;
			buffer[i * 4 + 1] = (rgba >> 8) & 0xff;
			buffer[i * 4 + 2] = (rgba >> 16) & 0xff;
			buffer[i * 4 + 3] = 0xff;
		}
	}
	texture->recreateFromImageData();
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
			FillTexture(mesh_, intersection, picker_->current_color());
		}
	};

	auto set_click_state = [&](bool new_val) {
		InputEvent *ie = (InputEvent*)e;
		if (ie->mouseButton == kMouseLeftButtonCode)
			left_clicking_ = new_val;
	};

	switch (e->getEventCode()) {
	case InputEvent::EVENT_MOUSEMOVE:
		if (left_clicking_)
			paint();
		break;
	case InputEvent::EVENT_MOUSEDOWN:
		paint();
		set_click_state(true);
		break;
	case InputEvent::EVENT_MOUSEUP:
		set_click_state(false);
		break;
	}
}

}