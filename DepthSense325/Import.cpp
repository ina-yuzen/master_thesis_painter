#include "Import.h"

#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <chrono>
#include <unordered_map>
#include <iostream>

#if _MSC_VER
#include <filesystem>
namespace filesystem = std::tr2::sys;
#else
#include <boost/filesystem.hpp>
namespace filesystem = boost::filesystem;
#endif

namespace mobamas {

class EnhSceneMesh : public Polycode::SceneMesh {
public:
	EnhSceneMesh(int mesh_type) : SceneMesh(mesh_type), 
		original_position_array_(Polycode::RenderDataArray::VERTEX_DATA_ARRAY),
		original_normal_array_(Polycode::RenderDataArray::NORMAL_DATA_ARRAY) {}

	void saveOriginal() {
		original_position_array_ = mesh->vertexPositionArray;
		original_normal_array_ = mesh->vertexNormalArray;
	}

	Polycode::VertexDataArray const& original_position_array() { return original_position_array_; }

	Polycode::VertexDataArray const& original_normal_array() { return original_normal_array_; }

private:
	Polycode::VertexDataArray original_position_array_;
	Polycode::VertexDataArray original_normal_array_;
};

struct BoneAssignment {
	float weights[4];
	unsigned int boneIds[4];
};
typedef std::vector<std::vector<BoneAssignment>> BoneAssignments;

aiMatrix4x4 getFullTransform(const struct aiNode *nd) {
	if (nd->mParent) {
		return  getFullTransform(nd->mParent) * nd->mTransformation;
	}
	else {
		return nd->mTransformation;
	}
}

Polycode::Material* createMaterial(std::string name) {
	Polycode::ResourcePool *globalPool = Polycode::Services()->getResourceManager()->getGlobalPool();
	auto mat = new Polycode::Material(name);
	auto shader = (Polycode::Shader*)globalPool->getResource(Polycode::Resource::RESOURCE_SHADER, "DefaultShader");
	if (shader == nullptr) {
		throw std::runtime_error("Failed to load shader");
	}
	mat->addShader(shader, shader->createBinding());
	return mat;
}

MeshGroup::MeshGroup() : Polycode::Entity(), wrapper_(new Polycode::Entity()) {
	addChild(wrapper_);
}

// FIXME: misbehave against models without bone weights
void MeshGroup::applyBoneMotion() {
	if (!skeleton_)
		return;
	skeleton_->Update();
	for (auto child : getSceneMeshes()) {
		auto mesh = static_cast<EnhSceneMesh*>(child);
		auto raw = mesh->getMesh();
		auto parr = mesh->original_position_array();
		auto narr = mesh->original_normal_array();
		for (int vidx = 0; vidx < raw->vertexPositionArray.data.size() / 3; vidx++) {
			Polycode::Vector3 rest_vert(parr.data[vidx * 3],
				parr.data[vidx * 3 + 1],
				parr.data[vidx * 3 + 2]);
			Polycode::Vector3 nvec(narr.data[vidx * 3], 
				narr.data[(vidx * 3) + 1], 
				narr.data[(vidx * 3) + 2]);
			Polycode::Vector3 real_pos, norm;
			for (int b = 0; b < 4; b++)
			{
				auto bidx = vidx * 4 + b;
				auto weight = raw->vertexBoneWeightArray.data[bidx];
				if (weight > 0.0) {
					auto bone = getSkeleton()->getBone(raw->vertexBoneIndexArray.data[bidx]);
					real_pos += bone->finalMatrix * rest_vert * weight;
					norm += bone->finalMatrix.rotateVector(nvec) * weight;
				}
			}
			raw->vertexPositionArray.data[vidx * 3] = real_pos.x;
			raw->vertexPositionArray.data[vidx * 3 + 1] = real_pos.y;
			raw->vertexPositionArray.data[vidx * 3 + 2] = real_pos.z;

			norm.Normalize();
			raw->vertexNormalArray.data[vidx * 3] = norm.x;
			raw->vertexNormalArray.data[vidx * 3 + 1] = norm.y;
			raw->vertexNormalArray.data[vidx * 3 + 2] = norm.z;
		}
	}
}

void MeshGroup::centralize() {
	const float inf = std::numeric_limits<float>::infinity();
	Polycode::Vector3 min(inf, inf, inf), max(-inf, -inf, -inf);
	for (auto child : getSceneMeshes()) {
		auto mesh = static_cast<EnhSceneMesh*>(child);
		auto raw = mesh->getMesh();
		for (int vidx = 0; vidx < raw->vertexPositionArray.data.size() / 3; vidx++) {
			float x = raw->vertexPositionArray.data[vidx * 3],
				y = raw->vertexPositionArray.data[vidx * 3 + 1],
				z = raw->vertexPositionArray.data[vidx * 3 + 2];
			if (x < min.x)
				min.x = x;
			if (y < min.y)
				min.y = y;
			if (z < min.z)
				min.z = z;
			if (x > max.x)
				max.x = x;
			if (y > max.y)
				max.y = y;
			if (z > max.z)
				max.z = z;
		}
	}
	// currently, consider only Y
	wrapper_->setPositionY(-(min.y + max.y) / 2);
}

void MeshGroup::addSceneMesh(Polycode::SceneMesh* newChild) {
	wrapper_->addChild(newChild);
}

std::vector<Polycode::SceneMesh*> MeshGroup::getSceneMeshes() {
	std::vector<Polycode::SceneMesh*> result;
	size_t count = wrapper_->getNumChildren();
	result.resize(count);
	for (int i = 0, n = count; i < n; ++i) {
		result[i] = static_cast<Polycode::SceneMesh*>(wrapper_->getChildAtIndex(i));
	}
	return result;
}

class ModelLoader {
public:
	ModelLoader(std::string path) : file_path_(path) {}
	MeshGroup* loadMesh();

private:
	const filesystem::path file_path_;
	const struct aiScene* sc_;
	MeshGroup* group_;
	BoneAssignments bone_assignments_;
	bool bone_assignments_cached_;
	int mesh_index_ = -1;
	bool has_weight_;
	std::unordered_map<const char*, unsigned int> bone_id_cache_;

	unsigned int ModelLoader::getBoneID(aiString const& name);
	void buildMesh(const struct aiNode* nd);
	Polycode::Bone* buildSkeleton(Polycode::Bone *parent, const struct aiNode* nd);
	void loadBoneAssignmentsCache();
	void saveBoneAssignmentsCache();
};

unsigned int ModelLoader::getBoneID(aiString const& name) {
	auto cname = name.C_Str();
	auto found = bone_id_cache_.find(cname);
	if (found == bone_id_cache_.end()) {
		auto skel = group_->getSkeleton();
		auto b = skel->getBoneByName(cname);
		assert(b != nullptr);
		auto id = skel->getBoneIndexByBone(b);
		bone_id_cache_[cname] = id;
		return id;
	} else {
		return found->second;
	}
}

void ModelLoader::buildMesh(const struct aiNode* nd) {
	int i, nIgnoredPolygons = 0;

	// draw all meshes assigned to this node
	for (size_t n = 0; n < nd->mNumMeshes; ++n) {
		EnhSceneMesh *scene_mesh = new EnhSceneMesh(Polycode::Mesh::TRI_MESH);
		Polycode::Mesh *tmesh = scene_mesh->getMesh();
		tmesh->indexedMesh = true;

		const aiMesh* mesh = sc_->mMeshes[nd->mMeshes[n]];
		mesh_index_++;

		std::cout << "Importing mesh " << mesh_index_ << ": ";
		std::cout.flush();
#ifdef _WINDOWS
		WStr w;
		utf8toWStr(w, mesh->mName.C_Str());
		std::wcout << w;
		std::wcout.flush();
#else
		std::cout << std::string(mesh->mName.C_Str());
		std::cout.flush();
#endif
		std::cout << " (" << mesh->mNumVertices << " vertices)"
			<< " (" << mesh->mNumFaces << " faces)" << std::endl;

		if (bone_assignments_cached_ && bone_assignments_[mesh_index_].size() != mesh->mNumVertices) {
			std::cerr << "Bone assignments and vertices count mismatch. Invalidating cache." << std::endl;
			bone_assignments_cached_ = false;
		}
		if (!bone_assignments_cached_) {
			bone_assignments_.resize(mesh_index_ + 1);
			bone_assignments_[mesh_index_].reserve(mesh->mNumVertices);
		}
		for (size_t t = 0; t < mesh->mNumVertices; ++t) {
			int index = t;
			if (mesh->mColors[0] != NULL) {
				tmesh->addColor(mesh->mColors[0][index].r, mesh->mColors[0][index].g, mesh->mColors[0][index].b, mesh->mColors[0][index].a);
			}

			if (mesh->mTangents != NULL)  {
				tmesh->addTangent(mesh->mTangents[index].x, mesh->mTangents[index].y, mesh->mTangents[index].z);
			}

			if (mesh->mNormals != NULL)  {
				tmesh->addNormal(mesh->mNormals[index].x, mesh->mNormals[index].y, mesh->mNormals[index].z);
			}

			if (mesh->HasTextureCoords(0)) {
				tmesh->addTexCoord(mesh->mTextureCoords[0][index].x, mesh->mTextureCoords[0][index].y);
			}

			if (mesh->HasTextureCoords(1)) {
				tmesh->addTexCoord2(mesh->mTextureCoords[1][index].x, mesh->mTextureCoords[1][index].y);
			}

			BoneAssignment ass;
			if (bone_assignments_cached_) {
				ass = bone_assignments_[mesh_index_][t];
			} else {
				int numAssignments = 0;

				for (unsigned int a = 0; a < mesh->mNumBones; a++) {
					aiBone* bone = mesh->mBones[a];
					unsigned int boneIndex = getBoneID(bone->mName);

					for (unsigned int b = 0; b < bone->mNumWeights && numAssignments < 4; b++) {
						if (bone->mWeights[b].mVertexId == index) {
							ass.weights[numAssignments] = bone->mWeights[b].mWeight;
							ass.boneIds[numAssignments] = boneIndex;
							numAssignments++;
						}
					}
				}	
				for (; numAssignments < 4; ++numAssignments) {
					ass.weights[numAssignments] = 0;
					ass.boneIds[numAssignments] = 0;
				}
				bone_assignments_[mesh_index_].push_back(ass);
			}

			if (!has_weight_) {
				for (size_t i = 0; i < 4; i++) {
					has_weight_ = ass.weights[i] != 0;
					break;
				}
			}
			tmesh->addBoneAssignments(
				ass.weights[0], ass.boneIds[0],
				ass.weights[1], ass.boneIds[1],
				ass.weights[2], ass.boneIds[2],
				ass.weights[3], ass.boneIds[3]);
			tmesh->addVertex(mesh->mVertices[index].x, mesh->mVertices[index].y, mesh->mVertices[index].z);
		}
		scene_mesh->saveOriginal();

		for (size_t t = 0; t < mesh->mNumFaces; ++t) {
			const struct aiFace* face = &mesh->mFaces[t];
			if (face->mNumIndices != 3) {
				nIgnoredPolygons++;
				continue;
			}

			for (i = 0; i < face->mNumIndices; i++) {
				int index = face->mIndices[i];
				tmesh->addIndex(index);
			}
		}

		if (tmesh->vertexBoneIndexArray.getDataSize() > 0) {
			tmesh->normalizeBoneWeights();
		}


		aiVector3D p;
		aiVector3D s;
		aiQuaternion r;

		aiMatrix4x4 fullTransform = getFullTransform(nd);

		// TODO: set position, scale, rotation if required
		fullTransform.Decompose(s, r, p);

		scene_mesh->setLocalBoundingBox(tmesh->calculateBBox());

		auto ai_mat = sc_->mMaterials[mesh->mMaterialIndex];
		int tex_index = 0;
		aiString tex_location;
		if (AI_SUCCESS == ai_mat->GetTexture(aiTextureType_DIFFUSE, tex_index, &tex_location)) {
			filesystem::path tex_path(tex_location.C_Str());
			if (!tex_path.is_complete()) {
				tex_path = file_path_.parent_path();
				tex_path /= tex_location.C_Str();
			}
			std::string path = tex_path;
			// MUST be before setMaterial()
			int wchars_num = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
			wchar_t* wstr = new wchar_t[wchars_num];
			MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wstr, wchars_num);
			scene_mesh->loadTexture(wstr);
			delete[] wstr;
		}

		std::string mat_name = "";
		aiString str;
		if (AI_SUCCESS == ai_mat->Get(AI_MATKEY_NAME, str)) {
			mat_name = std::string(str.C_Str());
		}
		auto poly_mat = createMaterial(mat_name);
		scene_mesh->setMaterial(poly_mat);
		Polycode::ShaderBinding *binding = scene_mesh->getLocalShaderOptions();
		aiColor4D color;
		if (AI_SUCCESS == ai_mat->Get(AI_MATKEY_COLOR_DIFFUSE, color)) {
			binding
				->addParam(Polycode::ProgramParam::PARAM_COLOR, "diffuse_color")
				->setColor(Polycode::Color(color.r, color.g, color.b, color.a));
		}
		if (AI_SUCCESS == ai_mat->Get(AI_MATKEY_COLOR_SPECULAR, color)) {
			binding
				->addParam(Polycode::ProgramParam::PARAM_COLOR, "specular_color")
				->setColor(Polycode::Color(color.r, color.g, color.b, color.a));
		}
		if (AI_SUCCESS == ai_mat->Get(AI_MATKEY_COLOR_AMBIENT, color)) {
			binding
				->addParam(Polycode::ProgramParam::PARAM_COLOR, "ambient_color")
				->setColor(Polycode::Color(color.r, color.g, color.b, color.a));
		}
		float val;
		if (AI_SUCCESS == ai_mat->Get(AI_MATKEY_SHININESS, val)) {
			binding
				->addParam(Polycode::ProgramParam::PARAM_NUMBER, "shininess")
				->setColor(val);
		}

		if (nIgnoredPolygons) {
			std::cerr << "Ignored " << nIgnoredPolygons << " non-triangular polygons" << std::endl;
		}

		// FIXME: 親子関係を崩してしまっているのでモデルによっては相対になってる行列が壊れるかも
		group_->addSceneMesh(scene_mesh);
	}

	// drill down to all children
	for (size_t n = 0; n < nd->mNumChildren; ++n) {
		buildMesh(nd->mChildren[n]);
	}
}

Polycode::Bone* ModelLoader::buildSkeleton(Polycode::Bone *parent, const struct aiNode* nd) {
	auto name = nd->mName;
	auto *bone = new Polycode::Bone(name.C_Str());
	bone->setParentBone(parent);
	bone->parentBoneId = group_->getSkeleton()->getBoneIndexByBone(parent);
	bone->disableAnimation = true;

	aiVector3D s;
	aiQuaternion rq;
	aiVector3D t;
	nd->mTransformation.Decompose(s, rq, t);

	bone->baseRotation = Polycode::Quaternion(rq.w, rq.x, rq.y, rq.z);
	bone->baseScale = Polycode::Vector3(s.x, s.y, s.z);
	bone->basePosition = Polycode::Vector3(t.x, t.y, t.z);

	bone->setPosition(t.x, t.y, t.z);
	bone->setRotationQuat(rq.w, rq.x, rq.y, rq.z);
	bone->setScale(s.x, s.y, s.z);
	bone->rebuildTransformMatrix();

	bone->setBaseMatrix(bone->getTransformMatrix());
	bone->setBoneMatrix(bone->getTransformMatrix());

	aiBone* found_bone = nullptr;
	for (unsigned int i = 0; i < sc_->mNumMeshes; i++) {
		auto mesh = sc_->mMeshes[i];
		for (unsigned int i = 0; i < mesh->mNumBones; i++) {
			auto ref = mesh->mBones[i];
			if (name == ref->mName) {
				if (found_bone) {
					WStr dst;
					utf8toWStr(dst, name.C_Str());
					std::wcerr << "Two references to the same bone " << dst << ". offsetMatrix conflicts." << std::endl;
				} else {
					found_bone = ref;
				}
			}
		}
	}
	if (found_bone) {
		found_bone->mOffsetMatrix.Decompose(s, rq, t);
		Polycode::Matrix4 m = Polycode::Quaternion(rq.w, rq.x, rq.y, rq.z).createMatrix();
		m.setPosition(t.x, t.y, t.z);
		bone->setRestMatrix(m);
	}

	group_->getSkeleton()->addBone(bone);
	for (int n = 0; n < nd->mNumChildren; ++n) {
		bone->addChildBone(buildSkeleton(bone, nd->mChildren[n]));
	}
	return bone;
}

void ModelLoader::loadBoneAssignmentsCache() {
	std::ifstream fs((std::string)file_path_ + ".bac", std::ios_base::binary | std::ios_base::in);
	if (!fs.is_open()) {
		bone_assignments_cached_ = false;
		return;
	}
	std::cout << "Cache file exists. Loading..." << std::endl;
	int n_meshes;
	if (!fs.read(reinterpret_cast<char*>(&n_meshes), sizeof(int))) {
		return;
	}
	bone_assignments_.reserve(n_meshes);
	for (int i = 0; i < n_meshes; ++i) {
		int n_vs;
		if (!fs.read(reinterpret_cast<char*>(&n_vs), sizeof(int))) {
			return;
		}
		std::vector<BoneAssignment> ass(n_vs);
		for (int j = 0; j < n_vs; ++j) {
			auto& as = ass[j];
			for (auto& w : as.weights) {
				if (!fs.read(reinterpret_cast<char*>(&w), sizeof(float))) {
					return;
				}
			}
			for (auto& id : as.boneIds) {
				if (!fs.read(reinterpret_cast<char*>(&id), sizeof(unsigned int))) {
					return;
				}
			}
		}
		bone_assignments_.push_back(ass);
	}
	bone_assignments_cached_ = true;
}

void ModelLoader::saveBoneAssignmentsCache() {
	if (bone_assignments_cached_)
		return;

	std::cout << "Creating bone assignments cache file." << std::endl;
	std::ofstream fs((std::string)file_path_ + ".bac", std::ios_base::binary | std::ios_base::out);
	int size = bone_assignments_.size();
	if (!fs.write(reinterpret_cast<char*>(&size), sizeof(int))) {
		return;
	}
	for (auto const& ass : bone_assignments_) {
		int n_vs = ass.size();
		if (!fs.write(reinterpret_cast<char*>(&n_vs), sizeof(int))) {
			return;
		}
		for (auto const& as : ass) {
			for (auto& w : as.weights) {
				if (!fs.write(reinterpret_cast<const char*>(&w), sizeof(float))) {
					return;
				}
			}
			for (auto& id : as.boneIds) {
				if (!fs.write(reinterpret_cast<const char*>(&id), sizeof(unsigned int))) {
					return;
				}
			}
		}
	}
}

MeshGroup* ModelLoader::loadMesh() {
	Assimp::Importer importer;
	sc_ = importer.ReadFile(file_path_, aiProcess_Triangulate | aiProcess_ValidateDataStructure | aiProcess_FindInvalidData);
	if (sc_ == nullptr) {
		std::cerr << "Failed to load " << file_path_ << " by Assimp::Importer " << importer.GetErrorString() << std::endl;
		return nullptr;
	}

	group_ = new MeshGroup();
	group_->setSkeleton(new Polycode::Skeleton());

	// skeleton must be built first to construct bone id list
	buildSkeleton(NULL, sc_->mRootNode);
	has_weight_ = false;
	loadBoneAssignmentsCache();
	buildMesh(sc_->mRootNode);
	saveBoneAssignmentsCache();
	if (!has_weight_)
		group_->setSkeleton(nullptr);

	if (sc_->HasAnimations()) {
		std::cerr << "Animation is not yet supported." << std::endl;
	}

	// init position
	group_->applyBoneMotion();

	return group_;
}

MeshGroup* importCollada(std::string path) {
	ModelLoader loader(path);
	return loader.loadMesh();
}

}