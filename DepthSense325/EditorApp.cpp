#include "EditorApp.h"

namespace mobamas {

EditorApp::EditorApp(PolycodeView *view): EventHandler(), left_clicking_(false) {
	core_ = new POLYCODE_CORE(view, 640, 480, false, true, 0, 0, 30);

	auto rm = Polycode::CoreServices::getInstance()->getResourceManager();
	rm->addArchive("Resources/default.pak");
	rm->addDirResource("default", false);

	scene_ = new Polycode::Scene();
	mesh_ = new SceneMesh("Resources/dummy.mesh");
	mesh_->loadTexture("Resources/dummy.png");
	mesh_->overlayWireframe = true;
	scene_->addEntity(mesh_);

	mesh_->loadSkeleton("Resources/dummy.skeleton");
	auto skel = mesh_->getSkeleton();
	for (int idx = 0; idx < skel->getNumBones(); idx++)
	{
		skel->getBone(idx)->disableAnimation = true;
	}

	auto cam = scene_->getDefaultCamera();
	cam->setPosition(5, 5, 5);
	cam->lookAt(Polycode::Vector3(0, 1, 0));

	auto input = core_->getInput();
	input->addEventListener(this, InputEvent::EVENT_MOUSEMOVE);
	input->addEventListener(this, InputEvent::EVENT_MOUSEDOWN);
	input->addEventListener(this, InputEvent::EVENT_MOUSEUP);
}

EditorApp::~EditorApp() {
	if (mesh_ != nullptr)
		delete mesh_;
	if (core_ != nullptr)
		delete core_;
}

bool EditorApp::Update() {
	auto ticks = core_->getTicks();
	//mesh_->getSkeleton()->getBone(0)->setPosition(0, sin(ticks * 0.1), 0);
	return core_->updateAndRender();
}

static int FindIntersectionPolygon(Polycode::SceneMesh *mesh, Polycode::Ray ray) {
	auto raw = mesh->getMesh();
	int vector_idx = -1;
	double distance = 1e10;

	std::vector<Polycode::Vector3> adjusted_points;
	for (int vidx=0; vidx < raw->vertexPositionArray.data.size()/3; vidx ++) {
		auto rest_vert = Polycode::Vector3(raw->vertexPositionArray.data[vidx*3],
			raw->vertexPositionArray.data[vidx*3+1],
			raw->vertexPositionArray.data[vidx*3+2]);
		Polycode::Vector3 real_pos;
		for (int b = 0; b < 4; b++)
		{
			auto bidx = vidx * 4 + b;
			auto weight = raw->vertexBoneWeightArray.data[bidx];
			if (weight > 0.0) {
				auto bone = mesh->getSkeleton()->getBone(raw->vertexBoneIndexArray.data[bidx]);
				real_pos += bone->finalMatrix * rest_vert * weight;
			}
		}
		adjusted_points.push_back(real_pos);
	}

	int step = raw->getIndexGroupSize();
	for (int ii = 0; ii < raw->getIndexCount(); ii += step) {
		auto idx0 = raw->indexArray.data[ii],
			idx1 = raw->indexArray.data[ii+1],
			idx2 = raw->indexArray.data[ii+2];
		if(ray.polygonIntersect(adjusted_points[idx0], adjusted_points[idx1], adjusted_points[idx2])) {
			auto this_dist = adjusted_points[idx0].distance(ray.origin);
			if (distance > this_dist) {
				vector_idx = ii;
				distance = this_dist;
			}
		}
	}
	return vector_idx;
}

typedef std::pair<int ,int> TP;
static int Interpolate(TP start, TP end, int y) {
	double ratio = (y - start.second) / static_cast<double>(end.second - start.second);
	return start.first * (1 - ratio) + end.first * ratio;
}

static void FillTexture(Polycode::SceneMesh *mesh, int iidx) {
	std::vector<TP> region;
	auto texture = mesh->getTexture();
	auto height = texture->getHeight(), width = texture->getWidth();
	for (int off = 0; off < 3; off ++) {
		auto coord = mesh->getMesh()->getVertexTexCoord(mesh->getMesh()->indexArray.data[iidx + off]);
		region.push_back(TP(width * coord.x, height * coord.y));
	}
	std::sort(region.begin(), region.end(), [](TP l, TP r) { return l.second < r.second; });
	auto buffer = texture->getTextureData();
	for (int y = region[0].second; y <= region[2].second; y ++) {
		int x1= Interpolate(region[0], region[2], y),
			x2;
		if (y < region[1].second) {
			x2 = Interpolate(region[0], region[1], y);
		} else {
			x2 = Interpolate(region[1], region[2], y);
		}
		if (x1 > x2) {
			std::swap(x1, x2);
		}
		for (int x = x1; x < x2; x ++) {
			int i = y * width + x;
			buffer[i * 4    ] = 0xff;
			buffer[i * 4 + 1] = 0;
			buffer[i * 4 + 2] = 0;
			buffer[i * 4 + 3] = 0xff;
		}
	}
	texture->recreateFromImageData();
}

void EditorApp::handleEvent(Event *e) {
	if (e->getDispatcher() != core_->getInput())
		return;

	InputEvent *ie = (InputEvent*)e;
	auto paint = [&]() {
		auto ray = scene_->projectRayFromCameraAndViewportCoordinate(scene_->getActiveCamera(), ie->getMousePosition());
		auto raw = mesh_->getMesh();
		assert(raw->getMeshType() == Polycode::Mesh::TRI_MESH);
		auto idx = FindIntersectionPolygon(mesh_, ray);
		if (idx == -1)
			return;
		FillTexture(mesh_, idx);
	};

	switch (e->getEventCode()) {
	case InputEvent::EVENT_MOUSEMOVE:
		if (left_clicking_)
			paint();
		break;
	case InputEvent::EVENT_MOUSEDOWN:
		paint();
		left_clicking_ = true;
		break;
	case InputEvent::EVENT_MOUSEUP:
		left_clicking_ = false;
		break;
	}
}

}