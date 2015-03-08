#include "ModelPainter.h"
#include "ColorPicker.h"

namespace mobamas {

ModelPainter::ModelPainter(Polycode::Scene *scene, Polycode::SceneMesh *mesh) :
	EventHandler(),
	scene_(scene),
	mesh_(mesh),
	left_clicking_(false)
{
	picker_.reset(new ColorPicker());

	auto input = Polycode::CoreServices::getInstance()->getInput();
	using Polycode::InputEvent;
	input->addEventListener(this, InputEvent::EVENT_MOUSEMOVE);
	input->addEventListener(this, InputEvent::EVENT_MOUSEDOWN);
	input->addEventListener(this, InputEvent::EVENT_MOUSEUP);
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

static void FillTexture(Polycode::SceneMesh *mesh, int iidx, Polycode::Color color) {
	std::vector<TP> region;
	auto texture = mesh->getTexture();
	auto height = texture->getHeight(), width = texture->getWidth();
	for (int off = 0; off < 3; off ++) {
		auto coord = mesh->getMesh()->getVertexTexCoord(mesh->getMesh()->indexArray.data[iidx + off]);
		region.push_back(TP(static_cast<int>(width * coord.x), static_cast<int>(height * coord.y)));
	}
	std::sort(region.begin(), region.end(), [](TP l, TP r) { return l.second < r.second; });
	auto buffer = texture->getTextureData();
	for (int y = region[0].second; y <= region[2].second; y ++) {
		int x1= Interpolate(region[0], region[2], y),
			x2;
		if (y < region[1].second || region[1].second == region[2].second) {
			x2 = Interpolate(region[0], region[1], y);
		} else {
			x2 = Interpolate(region[1], region[2], y);
		}
		if (x1 > x2) {
			std::swap(x1, x2);
		}
		for (int x = x1; x < x2; x ++) {
			int i = y * width + x;
			unsigned int rgba = color.getUint();
			buffer[i * 4    ] = rgba & 0xff;
			buffer[i * 4 + 1] = (rgba >> 8) & 0xff;
			buffer[i * 4 + 2] = (rgba >> 16) & 0xff;
			buffer[i * 4 + 3] = 0xff;
		}
	}
	texture->recreateFromImageData();
}

const int kMouseLeftButtonCode = 0;

void ModelPainter::handleEvent(Polycode::Event *e) {
	using Polycode::InputEvent;
	auto paint = [&]() {
		InputEvent *ie = (InputEvent*)e;
		auto ray = scene_->projectRayFromCameraAndViewportCoordinate(scene_->getActiveCamera(), ie->getMousePosition());
		auto raw = mesh_->getMesh();
		assert(raw->getMeshType() == Polycode::Mesh::TRI_MESH);
		auto idx = FindIntersectionPolygon(mesh_, ray);
		if (idx == -1)
			return;
		FillTexture(mesh_, idx, picker_->current_color());
	};

	auto set_click_state = [&](bool new_val) {
		InputEvent *ie = (InputEvent*)e;
		if (ie->mouseButton == kMouseLeftButtonCode)
			left_clicking_ = new_val;
	};

	switch (e->getEventCode()) {
	case InputEvent::EVENT_MOUSEMOVE:
		if (left_clicking_)
			paint();
		break;
	case InputEvent::EVENT_MOUSEDOWN:
		paint();
		set_click_state(true);
		break;
	case InputEvent::EVENT_MOUSEUP:
		set_click_state(false);
		break;
	}
}

}