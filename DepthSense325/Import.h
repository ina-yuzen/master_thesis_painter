#pragma once
#include <Polycode.h>

namespace mobamas {

class MeshGroup : public Polycode::Entity {
public:
	MeshGroup();
	void setSkeleton(Polycode::Skeleton* s) { skeleton_ = s; }
	Polycode::Skeleton* getSkeleton() { return skeleton_; }
	void applyBoneMotion();

	// adjust position to balance top and bottom region. Call right after first applyBoneMotion()
	void centralize();
	
	void addSceneMesh(Polycode::SceneMesh* newChild);
	std::vector<Polycode::SceneMesh*> getSceneMeshes();

private:
	Polycode::Entity* wrapper_;
	Polycode::Skeleton* skeleton_;
};

MeshGroup* importCollada(std::string path);

}
