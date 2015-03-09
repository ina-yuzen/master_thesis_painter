#pragma once
#include <Polycode.h>

namespace mobamas {

class BoneManipulation: public Polycode::EventHandler {
public:
	BoneManipulation(Polycode::Scene *scene, Polycode::SceneMesh *mesh);
	void handleEvent(Polycode::Event *e) override;
	void Update();
	
private:
	Polycode::Scene *scene_;
	Polycode::SceneMesh *mesh_;
	bool moving_;
};

}