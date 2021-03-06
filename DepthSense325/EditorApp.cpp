#include "EditorApp.h"

#include "BoneManipulation.h"
#include "Context.h"
#include "HandVisualization.h"
#include "Models.h"
#include "ModelRotation.h"
#include "ModelPainter.h"
#include "Import.h"
#include "PenPicker.h"
#include "Writer.h"

namespace mobamas {

MeshGroup* LoadMesh2(Models model) {
	switch (model) {
	case Models::MIKU:
	{
		auto group = importCollada("Resources/test4.dae");
		group->Scale(15);
		return group;
	}
	case Models::TREASURE:
	{
		auto group = importCollada("Resources/treasure.dae");
		//group->Scale(1);
		return group;
	}
	case Models::DOG:
	{
		auto group = importCollada("Resources/dog.dae");
		group->centralize();
		group->Scale(0.4);
		return group;
	}
	case Models::TV:
	{
		auto group = importCollada("Resources/tv.dae");
		group->centralize();
		group->Scale(0.4);
		return group;
	}
	case Models::BIRD:
	{
		auto group = importCollada("Resources/hato_ruto_b.dae");
		group->centralize();
		group->Scale(0.4);
		return group;
	}
	case Models::MIHON:
	{
		auto group = importCollada("Resources/test6.dae");
		group->Scale(15);
		return group;
	}
	default:
		std::cout << "Unknown model" << std::endl;
		assert(false);
		return nullptr;
	}
}

EditorApp::EditorApp(PolycodeView *view, std::shared_ptr<Context> context) : context_(context) {
	core_ = new POLYCODE_CORE(view, kWinWidth, kWinHeight, false, true, 0, 0, 90);

	core_->enableMouse(false);

	auto rm = Polycode::CoreServices::getInstance()->getResourceManager();
	rm->addArchive("Resources/default.pak");
	rm->addDirResource("default", false);

	auto scene = new Polycode::Scene();

	mesh_ = LoadMesh2(context->model);
	if (!mesh_) {
		throw std::runtime_error("Could not load mesh");
	}
	if (context->model == Models::MIKU) {
		mihon_ = LoadMesh2(Models::MIHON);
		if (!mihon_) {
			throw std::runtime_error("Could not load mesh");
		}
		auto skeleton = mihon_->getSkeleton();
		auto bone = skeleton->getBoneByName("right_arm");
		bone->Yaw(-50);
		bone->Pitch(20);
		bone = skeleton->getBoneByName("left_arm");
		bone->Pitch(-100);
		bone->Yaw(-40);
		mihon_->applyBoneMotion();
		scene->addEntity(mihon_); 
	}
	scene->addEntity(mesh_);
	scene->useClearColor = false;

	auto light = new SceneLight(SceneLight::POINT_LIGHT, scene, 100);
	light->setPosition(7, 7, 7);
	scene->addLight(light);
	light = new SceneLight(SceneLight::POINT_LIGHT, scene, 100);
	light->setPosition(-7, 7, 7);
	scene->addLight(light);
	light = new SceneLight(SceneLight::POINT_LIGHT, scene, 100);
	light->setPosition(7, -7, 7);
	scene->addLight(light);
	scene->enableLighting(true);

	auto cam = 	scene->getActiveCamera();
	cam->setPosition(0, 0, 5);
	cam->lookAt(Polycode::Vector3(0, 0, 0));

	if (context->operation_mode == OperationMode::MidAirMode || context->operation_mode == OperationMode::FrontMode) {
		hand_visualization_.reset(new HandVisualization(scene, context->rs_client));
	}
	auto picker = new PenPicker(context); // adjust event handler order
	bone_manipulation_.reset(new BoneManipulation(context, scene, mesh_, context->model, picker));
	painter_.reset(new ModelPainter(context, scene, mesh_, picker));
	rotation_.reset(new ModelRotation(context, mesh_, mihon_));

	context->pinch_listeners = bone_manipulation_;
}

EditorApp::~EditorApp() {
	if (mesh_ != nullptr)
		delete mesh_;
	if (core_ != nullptr)
		delete core_;
}

const unsigned int kAutoSaveDuration = 5000; // ms
bool EditorApp::Update() {
	bone_manipulation_->Update();
	if (hand_visualization_)
		hand_visualization_->Update();
	painter_->Update();
	auto tick = core_->getTicks();
	if (tick - last_save_tick_ > kAutoSaveDuration) {
		last_save_tick_ = tick;
		std::thread t([this]() {
			context_->writer->WritePose(mesh_->getSkeleton());
			for (auto sm : mesh_->getSceneMeshes()) {
				context_->writer->WriteTexture(sm->getTexture());
			}
		});
		t.detach();
	}
	return core_->updateAndRender();
}

void EditorApp::Shutdown() {
	painter_->Shutdown();
}

}