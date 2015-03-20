#pragma once
#include <memory>
#include <Polycode.h>

namespace mobamas {

struct Context;

class BackgroundImage
{
public:
	BackgroundImage(std::shared_ptr<Context> context);
	void Update();
private:
	std::shared_ptr<Context> context_;
	Polycode::Texture *bg_texture_;
};

}