#include "EditorApp.h"

#include "BoneManipulation.h"
#include "ModelRotation.h"
#include "ModelPainter.h"

namespace mobamas {

EditorApp::EditorApp(PolycodeView *view) {
	core_ = new POLYCODE_CORE(view, kWinWidth, kWinHeight, false, true, 0, 0, 90);

	core_->enableMouse(false);

	auto rm = Polycode::CoreServices::getInstance()->getResourceManager();
	rm->addArchive("Resources/default.pak");
	rm->addDirResource("default", false);

	auto scene = new Polycode::Scene();
	mesh_ = new SceneMesh("Resources/dummy.mesh");
	mesh_->loadTexture("Resources/dummy.png");
	mesh_->overlayWireframe = true;
	scene->addEntity(mesh_);

	mesh_->loadSkeleton("Resources/dummy.skeleton");
	auto skel = mesh_->getSkeleton();
	for (int idx = 0; idx < skel->getNumBones(); idx++)
	{
		skel->getBone(idx)->disableAnimation = true;
	}

	auto cam = 	scene->getActiveCamera();
	cam->setPosition(3, 3, 3);
	cam->lookAt(Polycode::Vector3(0, 0, 0));

	bone_manipulation_.reset(new BoneManipulation(scene, mesh_));
	painter_.reset(new ModelPainter(scene, mesh_));
	rotation_.reset(new ModelRotation(mesh_));
		
	auto input = core_->getInput();
}

EditorApp::~EditorApp() {
	if (mesh_ != nullptr)
		delete mesh_;
	if (core_ != nullptr)
		delete core_;
}

bool EditorApp::Update() {
	auto ticks = core_->getTicks();
	bone_manipulation_->Update();
	//mesh_->getSkeleton()->getBone(0)->setPosition(0, sin(ticks * 0.1), 0);
	return core_->updateAndRender();
}

}