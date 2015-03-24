#pragma once

#include <memory>
#include <Polycode.h>

namespace mobamas {
	
class RSClient;

class HandVisualization
{
public:
	HandVisualization(Polycode::Scene* scene, std::shared_ptr<RSClient> rs_client);
	void Update();

private:
	Polycode::Scene* scene_;
	std::shared_ptr<RSClient> rs_client_;
	Polycode::SceneMesh* mesh_;
};

}
