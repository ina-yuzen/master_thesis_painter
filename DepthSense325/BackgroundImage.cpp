#include "BackgroundImage.h"

#include "Context.h"
#include "DSClient.h"
#include "EditorApp.h"

namespace mobamas {

BackgroundImage::BackgroundImage(std::shared_ptr<Context> context) : context_(context) {
	auto bg = new Polycode::Scene(Polycode::Scene::SCENE_2D_TOPLEFT);
	auto bg_mesh = new Polycode::ScenePrimitive(Polycode::ScenePrimitive::TYPE_VPLANE, kWinWidth, kWinHeight);
	bg_mesh->loadTextureFromImage(new Polycode::Image(kColorWidth, kColorHeight, Polycode::Image::IMAGE_RGB));
	bg_mesh->setPosition(kWinWidth / 2, kWinHeight / 2);
	bg_texture_ = bg_mesh->getTexture();
	bg->addEntity(bg_mesh);
}

void BackgroundImage::Update() {
	auto new_data = context_->ds_client->LastColorData();
	if (!new_data.empty()) {
		auto data = bg_texture_->getTextureData();
		auto texture_mat = cv::Mat(kColorHeight, kColorWidth, CV_8UC3, data);
		cv::Mat flipped;
		cv::flip(new_data, flipped, 0);
		cv::cvtColor(flipped, texture_mat, CV_BGR2RGB);
		bg_texture_->recreateFromImageData();
	}
}

}