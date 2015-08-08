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

bool hasWeights = false;
std::vector<aiBone*> bones;
std::unordered_map<aiBone*, unsigned int> bones_inverted;
unsigned int numBones = 0;
filesystem::path file_path;

class RandomAccessSkeleton : public Polycode::Skeleton {
public:
	void addBone(Polycode::Bone* bone, size_t id) {
		if (bones.size() < id) {
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

unsigned int addBone(aiBone *bone) {
	auto found = bones_inverted.find(bone);
	if (found == bones_inverted.end()) {
		bones.push_back(bone);
		int id = bones.size() - 1;
		bones_inverted[bone] = id;
		return id;
	}
	else {
		return found->second;
	}
}

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

MeshGroup* buildMesh(const struct aiScene *sc, const struct aiNode* nd) {
	auto group = new MeshGroup();
	int i, nIgnoredPolygons = 0;

	// draw all meshes assigned to this node
	for (size_t n = 0; n < nd->mNumMeshes; ++n) {
		Polycode::SceneMesh *scene_mesh = new Polycode::SceneMesh(Polycode::Mesh::TRI_MESH);
		Polycode::Mesh *tmesh = scene_mesh->getMesh();
		tmesh->indexedMesh = true;

		const aiMesh* mesh = sc->mMeshes[nd->mMeshes[n]];

		std::cout << "Importing mesh:" << std::string(mesh->mName.C_Str())
			<< " (" << mesh->mNumVertices << " vertices)"
			<< " (" << mesh->mNumFaces << " faces)" << std::endl;

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

			int numAssignments = 0;

			float weights[4] = { 0.0, 0.0, 0.0, 0.0 };
			unsigned int boneIds[4] = { 0, 0, 0, 0 };

			// VERRRRRRRRRY heavy, file cache is required for production use
			for (unsigned int a = 0; a < mesh->mNumBones; a++) {
				aiBone* bone = mesh->mBones[a];
				unsigned int boneIndex = addBone(bone);

				for (unsigned int b = 0; b < bone->mNumWeights && numAssignments < 4; b++) {
					if (bone->mWeights[b].mVertexId == index) {
						weights[numAssignments] = bone->mWeights[b].mWeight;
						boneIds[numAssignments] = boneIndex;
						numAssignments++;
						hasWeights = true;
					}
				}
			}

			tmesh->addBoneAssignments(weights[0], boneIds[0], weights[1], boneIds[1], weights[2], boneIds[2], weights[3], boneIds[3]);
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

		scene_mesh->setLocalBoundingBox(tmesh->calculateBBox());

		auto ai_mat = sc->mMaterials[mesh->mMaterialIndex];
		int tex_index = 0;
		aiString tex_location;
		if (AI_SUCCESS == ai_mat->GetTexture(aiTextureType_DIFFUSE, tex_index, &tex_location)) {
			filesystem::path tex_path(tex_location.C_Str());
			if (!tex_path.is_complete()) {
				tex_path = file_path.parent_path();
				tex_path /= tex_location.C_Str();
			}
			std::string path = tex_path;
			// MUST be before setMaterial()
			int wchars_num = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
			wchar_t* wstr = new wchar_t[wchars_num];
			MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wstr, wchars_num);
			// do whatever with wstr
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

		group->addChild(scene_mesh);
	}

	// draw all children
	for (size_t n = 0; n < nd->mNumChildren; ++n) {
		group->addChild(buildMesh(sc, nd->mChildren[n]));
	}
	return group;
}

int getBoneID(aiString name) {
	for (int i = 0; i < bones.size(); i++) {
		if (bones[i]->mName == name) {
			return i;
		}
	}
	return 666;
}

void buildSkeleton(RandomAccessSkeleton *skel, Polycode::Bone *parent, const struct aiScene *sc, const struct aiNode* nd) {
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

	for (int i = 0; i < bones.size(); i++) {
		if (bones[i]->mName == name) {
			bones[i]->mOffsetMatrix.Decompose(s, rq, t);
			Polycode::Matrix4 m = Polycode::Quaternion(rq.w, rq.x, rq.y, rq.z).createMatrix();
			m.setPosition(t.x, t.y, t.z);
			bone->setRestMatrix(m);
		}
	}

	for (int n = 0; n < nd->mNumChildren; ++n) {
		buildSkeleton(skel, bone, sc, nd->mChildren[n]);
	}
	skel->addBone(bone, getBoneID(name));
}

Polycode::Entity* importCollada(std::string path) {
	file_path = filesystem::path(path);
	Assimp::Importer importer;
	auto scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_ValidateDataStructure | aiProcess_FindInvalidData);
	if (scene == nullptr) {
		std::cerr << "Failed to load " << path << " by Assimp::Importer" << std::endl;
		return nullptr;
	}

	MeshGroup *root = buildMesh(scene, scene->mRootNode);

	if (hasWeights) {
		auto *skeleton = new RandomAccessSkeleton();

		for (int n = 0; n < scene->mRootNode->mNumChildren; ++n) {
			if (scene->mRootNode->mChildren[n]->mNumChildren > 0) {
				buildSkeleton(skeleton, NULL, scene, scene->mRootNode->mChildren[n]);
			}
		}

		if (scene->HasAnimations()) {
			std::cerr << "Animation is not yet supported." << std::endl;
		}

		root->setSkeleton(skeleton);
	}

	root->Scale(0.3, 0.3, 0.3);
	root->Translate(0, -3, 0);

	return root;
}