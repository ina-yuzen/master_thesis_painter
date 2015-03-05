#include "ColorPicker.h"
#include "EditorApp.h"

namespace mobamas {

const std::vector<Polycode::Color> ColorPicker::kPalette = ([] {
	std::vector<Polycode::Color> p;
	p.push_back(Polycode::Color(0xffffffff));
	p.push_back(Polycode::Color(0xffff00ff));
	p.push_back(Polycode::Color(0xff6600ff));
	p.push_back(Polycode::Color(0xdd0000ff));

	p.push_back(Polycode::Color(0xff0099ff));
	p.push_back(Polycode::Color(0x330099ff));
	p.push_back(Polycode::Color(0x0000ccff));
	p.push_back(Polycode::Color(0x0099ffff));
	
	p.push_back(Polycode::Color(0x00aa00ff));
	p.push_back(Polycode::Color(0x006600ff));
	p.push_back(Polycode::Color(0x663300ff));
	p.push_back(Polycode::Color(0x996633ff));

	p.push_back(Polycode::Color(0xbbbbbbff));
	p.push_back(Polycode::Color(0x888888ff));
	p.push_back(Polycode::Color(0x444444ff));
	p.push_back(Polycode::Color(0x000000ff));
	return p;
})();

const int kSize = 30;
ColorPicker::ColorPicker(): Polycode::EventHandler() {
	scene_ = new Polycode::Scene(Polycode::Scene::SCENE_2D_TOPLEFT);
	scene_->getActiveCamera()->setOrthoSize(kWinWidth, kWinHeight);
	scene_->useClearColor = false;

	for (int idx = 0; idx < kPalette.size(); idx++)	{
		auto plane = new Polycode::ScenePrimitive(Polycode::ScenePrimitive::TYPE_VPLANE, kSize, kSize);
		plane->setPosition(
			kWinWidth - kSize * 2 - 20 + kSize * (idx / 8),
			kWinHeight - 20 - kSize * (1+idx % 8));
		plane->setColor(kPalette[idx]);

		scene_->addChild(plane);
	}
	current_color_ = kPalette.front();


	cursor_.reset(new Polycode::ScenePrimitive(Polycode::ScenePrimitive::TYPE_CIRCLE, 10, 10, 10));
	scene_->addChild(cursor_.get());
	
	auto input = CoreServices::getInstance()->getInput();
	input->addEventListener(this, InputEvent::EVENT_MOUSEDOWN);
	input->addEventListener(this, InputEvent::EVENT_MOUSEMOVE);
}

Polycode::Color* PickClickedColor(Polycode::SceneEntity root, Polycode::Vector2 point) {
	for (int ci = 0; ci < root.getNumChildren(); ci++)
	{
		auto color = root.getChildAtIndex(ci);
		auto pos = color->getPosition();
		if (pos.x - kSize / 2 <= point.x && pos.x + kSize / 2 >= point.x && 
			pos.y - kSize / 2 <= point.y && pos.y + kSize / 2 >= point.y) {
				return &color->color;
		}
	}
	return nullptr;
}

void ColorPicker::handleEvent(Polycode::Event *e) {
	using Polycode::InputEvent;
	switch (e->getEventCode()) {
	case InputEvent::EVENT_MOUSEDOWN: 
		{
			auto picked = PickClickedColor(scene_->rootEntity, ((InputEvent*)e)->getMousePosition());
			if (picked != nullptr) {
				current_color_ = *picked;
				cursor_->setColor(picked);
			}
		}
		break;
	case InputEvent::EVENT_MOUSEMOVE:
		auto point = ((InputEvent*)e)->getMousePosition();
		cursor_->setPosition(point.x, point.y);
		break;
	}
}

}