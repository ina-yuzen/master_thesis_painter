#pragma once

#include <Polycode.h>

namespace mobamas {

// http://geomalgorithms.com/a06-_intersect-2.html#intersect3D_RayTriangle()
struct Intersection {
	bool found;
	Polycode::SceneMesh *scene_mesh;
	int first_vertex_index;
	Polycode::Vector3 point;
};

Intersection CalculateIntersectionPoint(
	const Polycode::Ray& ray,
	const Polycode::Vector3& v0,
	const Polycode::Vector3& v1,
	const Polycode::Vector3& v2);
Intersection FindIntersectionPolygon(vector<Polycode::SceneMesh*> const& meshes, const Polycode::Ray& ray);

}
