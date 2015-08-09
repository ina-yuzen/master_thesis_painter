#include "EditorApp.h"

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

class RandomAccessSkeleton : public Polycode::Skeleton {
public:
	void addBone(Polycode::Bone* bone, size_t id) {
		if (bones.size() <= id) {
			bones.resize(id + 1);
		}
		bones[id] = bone;
	}
};

class MeshGroup : public Polycode::Entity {
public:
	using Polycode::Entity::Entity;
	void setSkeleton(Polycode::Skeleton* s) { skeleton = s; }
	Polycode::Skeleton* getSkeleton() { return skeleton; }

private:
	Polycode::Skeleton* skeleton;
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

class ModelLoader {
public:
	ModelLoader(std::string path) : file_path_(path) {}
	MeshGroup* loadMesh();
	
private:
	const filesystem::path file_path_;
	const struct aiScene* sc_;
	MeshGroup* group_;
	bool has_weights_ = false;
	std::vector<aiBone*> bones_;
	std::unordered_map<aiBone*, unsigned int> bones_inverted_;
	unsigned int num_bones_ = 0;
	BoneAssignments bone_assignments_;
	bool bone_assignments_cached_;
	int mesh_index_ = -1;

	unsigned int addBone(aiBone *bone);
	unsigned int ModelLoader::getBoneID(aiString name);
	void buildMesh(const struct aiNode* nd);
	void buildSkeleton(RandomAccessSkeleton *skel, Polycode::Bone *parent, const struct aiNode* nd);
	void loadBoneAssignmentsCache();
	void saveBoneAssignmentsCache();
};

unsigned int ModelLoader::addBone(aiBone *bone) {
	auto found = bones_inverted_.find(bone);
	if (found == bones_inverted_.end()) {
		bones_.push_back(bone);
		int id = bones_.size() - 1;
		bones_inverted_[bone] = id;
		return id;
	} else {
		return found->second;
	}
}

unsigned int ModelLoader::getBoneID(aiString name) {
	for (unsigned int i = 0; i < bones_.size(); i++) {
		if (bones_[i]->mName == name) {
			return i;
		}
	}
	auto dummy = new aiBone();
	dummy->mName = name;
	return addBone(dummy);
}

void ModelLoader::buildMesh(const struct aiNode* nd) {
	int i, nIgnoredPolygons = 0;

	// draw all meshes assigned to this node
	for (size_t n = 0; n < nd->mNumMeshes; ++n) {
		Polycode::SceneMesh *sc__mesh = new Polycode::SceneMesh(Polycode::Mesh::TRI_MESH);
		Polycode::Mesh *tmesh = sc__mesh->getMesh();
		tmesh->indexedMesh = true;

		const aiMesh* mesh = sc_->mMeshes[nd->mMeshes[n]];
		mesh_index_ ++;

		std::cout << "Importing mesh " << mesh_index_ << ":";
#ifdef _WINDOWS
		const char* ptr = mesh->mName.C_Str();
		int wchars_num = MultiByteToWideChar(CP_UTF8, 0, ptr, -1, NULL, 0);
		wchar_t* wstr = new wchar_t[wchars_num];
		MultiByteToWideChar(CP_UTF8, 0, ptr, -1, wstr, wchars_num);
		std::wcout << wstr;
		delete[] wstr;
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
			}
			else {
				int numAssignments = 0;

				// VERRRRRRRRRY heavy, file cache is required for production use
				for (unsigned int a = 0; a < mesh->mNumBones; a++) {
					aiBone* bone = mesh->mBones[a];
					unsigned int boneIndex = addBone(bone);

					for (unsigned int b = 0; b < bone->mNumWeights && numAssignments < 4; b++) {
						if (bone->mWeights[b].mVertexId == index) {
							ass.weights[numAssignments] = bone->mWeights[b].mWeight;
							ass.boneIds[numAssignments] = boneIndex;
							numAssignments++;
							has_weights_ = true;
						}
					}
				}
				bone_assignments_[mesh_index_].push_back(ass);
			}

			tmesh->addBoneAssignments(
				ass.weights[0], ass.boneIds[0], 
				ass.weights[1], ass.boneIds[1], 
				ass.weights[2], ass.boneIds[2], 
				ass.weights[3], ass.boneIds[3]);
			tmesh->addVertex(mesh->mVertices[index].x, mesh->mVertices[index].y, mesh->mVertices[index].z);
		}

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

		sc__mesh->setLocalBoundingBox(tmesh->calculateBBox());

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
			// do whatever with wstr
			sc__mesh->loadTexture(wstr);
			delete[] wstr;
		}

		std::string mat_name = "";
		aiString str;
		if (AI_SUCCESS == ai_mat->Get(AI_MATKEY_NAME, str)) {
			mat_name = std::string(str.C_Str());
		}
		auto poly_mat = createMaterial(mat_name);
		sc__mesh->setMaterial(poly_mat);
		Polycode::ShaderBinding *binding = sc__mesh->getLocalShaderOptions();
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

		group_->addChild(sc__mesh);
	}

	// drill down to all children
	for (size_t n = 0; n < nd->mNumChildren; ++n) {
		buildMesh(nd->mChildren[n]);
	}
}

void ModelLoader::buildSkeleton(RandomAccessSkeleton *skel, Polycode::Bone *parent, const struct aiNode* nd) {
	auto name = nd->mName;
	auto *bone = new Polycode::Bone(name.C_Str());
	bone->setParentBone(parent);

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

	for (int i = 0; i < bones_.size(); i++) {
		if (bones_[i]->mName == name) {
			bones_[i]->mOffsetMatrix.Decompose(s, rq, t);
			Polycode::Matrix4 m = Polycode::Quaternion(rq.w, rq.x, rq.y, rq.z).createMatrix();
			m.setPosition(t.x, t.y, t.z);
			bone->setRestMatrix(m);
		}
	}

	for (int n = 0; n < nd->mNumChildren; ++n) {
		buildSkeleton(skel, bone, nd->mChildren[n]);
	}
	skel->addBone(bone, getBoneID(name));
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
	has_weights_ = true;
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
		std::cerr << "Failed to load " << file_path_ << " by Assimp::Importer" << std::endl;
		return nullptr;
	}

	loadBoneAssignmentsCache();

	group_ = new MeshGroup();
	buildMesh(sc_->mRootNode);

	saveBoneAssignmentsCache();

	if (has_weights_) {
		auto *skeleton = new RandomAccessSkeleton();

		for (int n = 0; n < sc_->mRootNode->mNumChildren; ++n) {
			if (sc_->mRootNode->mChildren[n]->mNumChildren > 0) {
				buildSkeleton(skeleton, NULL, sc_->mRootNode->mChildren[n]);
			}
		}

		if (sc_->HasAnimations()) {
			std::cerr << "Animation is not yet supported." << std::endl;
		}

		group_->setSkeleton(skeleton);
	}

	group_->Scale(0.3, 0.3, 0.3);
	group_->Translate(0, -3, 0);

	return group_;
}

Polycode::Entity* importCollada(std::string path) {
	ModelLoader loader(path);
	return loader.loadMesh();
}