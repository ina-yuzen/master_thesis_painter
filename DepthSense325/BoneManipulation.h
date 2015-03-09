#pragma once
#include <Polycode.h>
#include <vector>

namespace mobamas {

struct BoneHandle {
	Polycode::Bone* bone;
	Polycode::SceneMesh *marker;
	unsigned int bone_id;
};

class BoneManipulation: public Polycode::EventHandler {
public:
	BoneManipulation(Polycode::Scene *scene, Polycode::SceneMesh *mesh);
	void handleEvent(Polycode::Event *e) override;
	void Update();
	
private:
	Polycode::Scene *scene_;
	Polycode::SceneMesh *mesh_;
	std::vector<BoneHandle> handles_;
	BoneHandle* current_target_;
	Polycode::Vector2 mouse_prev_;
};

}