#pragma once
#include <Polycode.h>

namespace mobamas {

class MeshGroup : public Polycode::Entity {
public:
	using Polycode::Entity::Entity;
	void setSkeleton(Polycode::Skeleton* s) { skeleton_ = s; }
	Polycode::Skeleton* getSkeleton() { return skeleton_; }
	void applyBoneMotion();

private:
	Polycode::Skeleton* skeleton_;
};

MeshGroup* importCollada(std::string path);

}
