#include "PenPicker.h"

#include "Context.h"
#include "EditorApp.h"
#include "PenAsMouse.h"
#include "Writer.h"

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
PenPicker::PenPicker(std::shared_ptr<Context> context): 
	context_(context),
	Polycode::EventHandler(), 
	current_brush_(Brush::PEN) 
{
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
		auto displaySize = DisplaySize(i);
		auto plane = new Polycode::ScenePrimitive(Polycode::ScenePrimitive::TYPE_CIRCLE, displaySize, displaySize, displaySize);
		plane->setPosition(kWinWidth - kSize - 20, kSize * i + 20);
		plane->setColor(1, 1, 1, 1);
		scene_->addChild(plane);
		size_picks_.push_back(plane);
	}
	current_size_ = 1;

	auto stamps = std::vector < std::string > {"Resources\\stamp0_heart.png"};
	for (int i = 0; i < stamps.size(); ++i)
	{
		auto plane = new Polycode::ScenePrimitive(Polycode::ScenePrimitive::TYPE_VPLANE, kSize, kSize);
		plane->setPosition(kWinWidth - kSize * 2 - 20, kSize * i + 20);
		plane->loadTexture(stamps[i]);
		scene_->addChild(plane);
		stamp_picks_.push_back(plane);
	}
	if (context->operation_mode == OperationMode::TouchMode) {
		MaterialManager *materialManager = CoreServices::getInstance()->getMaterialManager();
		pen_target_textures_[PenTarget::DRAW] = materialManager->createTextureFromFile("Resources\\hand-paper.png", materialManager->clampDefault, materialManager->mipmapsDefault);
		pen_target_textures_[PenTarget::BONE] = materialManager->createTextureFromFile("Resources\\hand-rock.png", materialManager->clampDefault, materialManager->mipmapsDefault);
		pen_target_pick_ = new Polycode::ScenePrimitive(Polycode::ScenePrimitive::TYPE_VPLANE, kSize, kSize);
		pen_target_pick_->setPosition(kWinWidth - kSize * 2 - 20, kSize * stamps.size() + 20);
		pen_target_pick_->setTexture(pen_target_textures_[pen_target_]);
		scene_->addChild(pen_target_pick_);
	}
	current_size_ = 1;

	cursor_.reset(new Polycode::ScenePrimitive(Polycode::ScenePrimitive::TYPE_CIRCLE, 10, 10, 10));
	scene_->addChild(cursor_.get());
	
	new PenAsMouse(this);
}

bool IsClicking(const Polycode::Vector2& point, const Polycode::SceneEntity* obj) {
	auto pos = obj->getPosition();
	return (pos.x - kSize / 2 <= point.x && pos.x + kSize / 2 >= point.x &&
		pos.y - kSize / 2 <= point.y && pos.y + kSize / 2 >= point.y);
}

void PenPicker::UpdateCursorStyle() {
	if (current_brush_ == Brush::STAMP) {
		cursor_->setPrimitiveOptions(Polycode::ScenePrimitive::TYPE_VPLANE, kSize, kSize);
		cursor_->setTexture(current_stamp());
		cursor_->setColor(1, 1, 1, 1);
	}
	else {
		auto size = DisplaySize(current_size());
		cursor_->setColor(current_color());
		cursor_->setPrimitiveOptions(Polycode::ScenePrimitive::TYPE_CIRCLE, size, size, size);
		cursor_->setTexture(nullptr);
	}
}

bool PenPicker::PickClicked(const Polycode::Vector2& point) {
	for (auto color_pick : color_picks_) {
		if (IsClicking(point, color_pick)) {
			current_brush_ = Brush::PEN;
			current_color_ = color_pick->color;
			context_->writer->log() << "PenColorChanged " << current_color_.r << ", " << current_color_.g << ", " << current_color_.b << std::endl;
			UpdateCursorStyle();
			return true;
		}
	}
	for (auto size_pick: size_picks_) {
		if (IsClicking(point, size_pick)) {
			current_brush_ = Brush::PEN;
			auto size = size_pick->getPrimitiveParameter1();
			current_size_ = (int)(size / 5) - 1;
			context_->writer->log() << "PenSizeChanged " << current_size_ << std::endl;
			UpdateCursorStyle();
			return true;
		}
	}
	for (auto pick: stamp_picks_) {
		if (IsClicking(point, pick)) {
			current_brush_ = Brush::STAMP;
			current_stamp_ = pick->getTexture();
			context_->writer->log() << "StampPicked" << std::endl;
			UpdateCursorStyle();
			return true;
		}
	}
	if (context_->operation_mode == OperationMode::TouchMode) {
		if (IsClicking(point, pen_target_pick_)) {
			pen_target_ = pen_target_ == PenTarget::BONE ? PenTarget::DRAW : PenTarget::BONE;
			pen_target_pick_->setTexture(pen_target_textures_[pen_target_]);
			context_->writer->log() << "Pen target is changed to " << (pen_target_ == PenTarget::BONE ? "BONE" : "DRAW") << std::endl;
			return true;
		}
	}
	return false;
}

void PenPicker::handleEvent(Polycode::Event *e) {
	using Polycode::InputEvent;
	auto ie = static_cast<InputEvent*>(e);
	switch (e->getEventCode()) {
	case InputEvent::EVENT_MOUSEDOWN: 
		if (ie->getMouseButton() == Polycode::CoreInput::MOUSE_BUTTON1)
			if (PickClicked(((InputEvent*)e)->getMousePosition())) {
				e->cancelEvent();
			}
		break;
	case InputEvent::EVENT_MOUSEMOVE:
		auto point = ((InputEvent*)e)->getMousePosition();
		cursor_->setPosition(point.x, point.y);
		break;
	}
}

}