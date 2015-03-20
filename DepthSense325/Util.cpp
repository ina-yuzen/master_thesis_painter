#include "Util.h"

namespace mobamas {

std::vector<Polycode::Vector3> ActualVertexPositions(Polycode::SceneMesh *mesh) {
	std::vector<Polycode::Vector3> result;
	auto raw = mesh->getMesh();
	result.reserve(raw->getVertexCount());
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
		result.push_back(mesh_transform * real_pos);
	}
	return result;
}

}