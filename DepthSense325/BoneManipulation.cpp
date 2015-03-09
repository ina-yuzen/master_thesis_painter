#include "BoneManipulation.h"

#include <functional>

namespace mobamas {

const int kMovableBonesDepth = 5;	

BoneManipulation::BoneManipulation(Polycode::Scene *scene, Polycode::SceneMesh *mesh) :
	EventHandler(),
	scene_(scene),
	mesh_(mesh),
	moving_(false)
{
	auto skeleton = mesh_->getSkeleton();
	Polycode::Bone *root = nullptr;
	std::map<Polycode::Bone*, int> bone_id_map;
	for (int bidx = 0; bidx < skeleton->getNumBones(); bidx++)
	{
		auto b = skeleton->getBone(bidx);
		bone_id_map[b] = bidx;
		if (b->getParentBone() == nullptr) {
			root = b;
		}
	}
	std::vector<Polycode::Vector3> adjusted_points;
	auto raw = mesh_->getMesh();
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
				auto bone = mesh_->getSkeleton()->getBone(raw->vertexBoneIndexArray.data[bidx]);
				real_pos += bone->finalMatrix * rest_vert * weight;
			}
		}
		adjusted_points.push_back(real_pos);
	}
	std::vector<Polycode::Vector3> bone_centers(skeleton->getNumBones(), Polycode::Vector3(0, 0, 0));
	std::vector<double> bone_weights(skeleton->getNumBones(), 0);
	auto indices = raw->vertexBoneIndexArray;
	auto weights = raw->vertexBoneWeightArray;
	for (int vi = 0; vi < indices.getDataSize(); vi++) {
		int bi = indices.data[vi];
		double w = weights.data[vi];
		if (w > 0.0) {
			bone_centers[bi] = bone_centers[bi] + adjusted_points[vi / 4] * w;
			bone_weights[bi] += w;
		}
	}
	for (int i = 0; i < bone_centers.size(); i++)
	{
		if (bone_weights[i] > 0.0)
			bone_centers[i] = bone_centers[i] / bone_weights[i];
	}
	std::function<void (Polycode::Bone* bone, int depth)> iterate = [&](Polycode::Bone* bone, int depth) -> void {
		if (depth >= kMovableBonesDepth || bone == nullptr)
			return;
		bone->rebuildFinalMatrix();
		auto marker = new Polycode::ScenePrimitive(Polycode::ScenePrimitive::TYPE_BOX, 0.3, 0.3, 0.3);
		marker->setPosition(bone->finalMatrix * bone_centers[bone_id_map[bone]]);
		marker->setColor(0.1 * depth, 0.3, 0.3, 0.5);
		marker->depthTest = false;
		scene_->addEntity(marker);
		int count = bone->getNumChildren();
		for (int i = 0; i < count; i++)
		{
			iterate(bone->getChildBone(i), depth + 1);
		}
	};
	iterate(root, 0);
	root->setPositionY(root->getPosition().y - 1);
	auto input = Polycode::CoreServices::getInstance()->getInput();
	using Polycode::InputEvent;
	input->addEventListener(this, InputEvent::EVENT_MOUSEMOVE);
	input->addEventListener(this, InputEvent::EVENT_MOUSEDOWN);
	input->addEventListener(this, InputEvent::EVENT_MOUSEUP);
}

static int FindIntersectionPolygon(Polycode::SceneMesh *mesh, Polycode::Ray ray) {
	auto raw = mesh->getMesh();
	int vector_idx = -1;
	double distance = 1e10;

	std::vector<Polycode::Vector3> adjusted_points;
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
		adjusted_points.push_back(real_pos);
	}

	int step = raw->getIndexGroupSize();
	for (int ii = 0; ii < raw->getIndexCount(); ii += step) {
		auto idx0 = raw->indexArray.data[ii],
			idx1 = raw->indexArray.data[ii+1],
			idx2 = raw->indexArray.data[ii+2];
		if(ray.polygonIntersect(adjusted_points[idx0], adjusted_points[idx1], adjusted_points[idx2])) {
			auto this_dist = adjusted_points[idx0].distance(ray.origin);
			if (distance > this_dist) {
				vector_idx = ii;
				distance = this_dist;
			}
		}
	}
	return vector_idx;
}

const int kMouseMiddleButtonCode = 0;

void BoneManipulation::handleEvent(Polycode::Event *e) {
	using Polycode::InputEvent;

	auto set_click_state = [&](bool new_val) {
		InputEvent *ie = (InputEvent*)e;
		if (ie->mouseButton == kMouseMiddleButtonCode)
			moving_ = new_val;
	};

	switch (e->getEventCode()) {
	case InputEvent::EVENT_MOUSEMOVE:
		break;
	case InputEvent::EVENT_MOUSEDOWN:
		set_click_state(true);
		break;
	case InputEvent::EVENT_MOUSEUP:
		set_click_state(false);
		break;
	}
}

void BoneManipulation::Update() {
}

}