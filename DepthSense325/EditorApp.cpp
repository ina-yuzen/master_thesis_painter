#include "EditorApp.h"

#include "BoneManipulation.h"
#include "Context.h"
#include "HandVisualization.h"
#include "ModelRotation.h"
#include "ModelPainter.h"

namespace mobamas {

EditorApp::EditorApp(PolycodeView *view, std::shared_ptr<Context> context) {
	core_ = new POLYCODE_CORE(view, kWinWidth, kWinHeight, false, true, 0, 0, 90);

	core_->enableMouse(false);

	auto rm = Polycode::CoreServices::getInstance()->getResourceManager();
	rm->addArchive("Resources/default.pak");
	rm->addDirResource("default", false);

	auto scene = new Polycode::Scene();
	mesh_ = new SceneMesh("Resources/dummy.mesh");
	mesh_->loadTexture("Resources/dummy.png");
	scene->addEntity(mesh_);
	scene->useClearColor = false;

	mesh_->loadSkeleton("Resources/dummy.skeleton");
	auto skeleton = mesh_->getSkeleton();
	Polycode::Bone* root = nullptr;
	for (unsigned int bidx = 0; bidx < skeleton->getNumBones(); bidx++)
	{
		auto b = skeleton->getBone(bidx);
		if (b->getParentBone() == nullptr) {
			root = b;
			break;
		}
	}
	root->setPositionY(root->getPosition().y - 1); // adjust center to rotate

	auto cam = 	scene->getActiveCamera();
	cam->setPosition(0, 0, 5);
	cam->lookAt(Polycode::Vector3(0, 0, 0));

	hand_visualization_.reset(new HandVisualization(scene, context->rs_client));
	bone_manipulation_.reset(new BoneManipulation(scene, mesh_));
	painter_.reset(new ModelPainter(scene, mesh_));
	rotation_.reset(new ModelRotation(mesh_));

	context->pinch_listeners = bone_manipulation_;
}

EditorApp::~EditorApp() {
	if (mesh_ != nullptr)
		delete mesh_;
	if (core_ != nullptr)
		delete core_;
}

bool EditorApp::Update() {
	bone_manipulation_->Update();
	hand_visualization_->Update();
	return core_->updateAndRender();
}

}