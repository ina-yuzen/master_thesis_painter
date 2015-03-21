#include "PenPicker.h"
#include "EditorApp.h"

namespace mobamas {

const std::vector<Polycode::Color> PenPicker::kPalette = ([] {
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

const int kMaxSizeStep = 4;

const int kSize = 30;
PenPicker::PenPicker(): Polycode::EventHandler() {
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
		color_picks_.push_back(plane);
	}
	current_color_ = kPalette.front();

	for (size_t i = 0; i < kMaxSizeStep; i++)
	{
		auto displaySize = (i + 1) * 5;
		auto plane = new Polycode::ScenePrimitive(Polycode::ScenePrimitive::TYPE_CIRCLE, displaySize, displaySize, displaySize);
		plane->setPosition(kWinWidth - kSize - 20, kSize * i + 20);
		plane->setColor(1, 1, 1, 1);
		scene_->addChild(plane);
		size_picks_.push_back(plane);
	}
	current_size_ = 1;

	cursor_.reset(new Polycode::ScenePrimitive(Polycode::ScenePrimitive::TYPE_CIRCLE, 10, 10, 10));
	scene_->addChild(cursor_.get());
	
	auto input = CoreServices::getInstance()->getInput();
	input->addEventListener(this, InputEvent::EVENT_MOUSEDOWN);
	input->addEventListener(this, InputEvent::EVENT_MOUSEMOVE);
}

bool IsClicking(const Polycode::Vector2& point, const Polycode::SceneEntity* obj) {
	auto pos = obj->getPosition();
	return (pos.x - kSize / 2 <= point.x && pos.x + kSize / 2 >= point.x &&
		pos.y - kSize / 2 <= point.y && pos.y + kSize / 2 >= point.y);
}

void PenPicker::PickClicked(const Polycode::Vector2& point) {
	auto root = scene_->rootEntity;
	for (auto color_pick : color_picks_) {
		if (IsClicking(point, color_pick)) {
			current_color_ = color_pick->color;
			cursor_->setColor(current_color_);
			break;
		}
	}
	for (auto size_pick: size_picks_) {
		if (IsClicking(point, size_pick)) {
			auto size = size_pick->getPrimitiveParameter1();
			current_size_ = (int)(size / 5) - 1;
			cursor_->setPrimitiveOptions(cursor_->getPrimitiveType(), size, size, size);
			break;
		}
	}
}

void PenPicker::handleEvent(Polycode::Event *e) {
	using Polycode::InputEvent;
	switch (e->getEventCode()) {
	case InputEvent::EVENT_MOUSEDOWN: 
		PickClicked(((InputEvent*)e)->getMousePosition());
		break;
	case InputEvent::EVENT_MOUSEMOVE:
		auto point = ((InputEvent*)e)->getMousePosition();
		cursor_->setPosition(point.x, point.y);
		break;
	}
}

}