#include "Intersection.h"

#include "Util.h"

namespace mobamas {

const double kEpsilon = 0.000001;

Intersection CalculateIntersectionPoint(const Polycode::Ray& ray,
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

Intersection FindIntersectionPolygon(vector<Polycode::SceneMesh*> const& meshes, const Polycode::Ray& ray) {
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

}
