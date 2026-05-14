#include "3d/Model.h"
#include "3d/ModelManager.h"
#include "base/SrvManager.h"
#include "2d/SpriteCommon.h" 
#include <cassert>
#include <filesystem>

/**
 * Assimpの行列(aiMatrix4x4)を独自の行列(Matrix4x4)に変換する
 * 資料スライド 9 に基づき、転置を行ってコピーします
 */
static Matrix4x4 ConvertAiMatrix(const aiMatrix4x4& aiMat) {
	Matrix4x4 result;
	// 要素のコピー
	result.m[0][0] = aiMat.a1; result.m[0][1] = aiMat.a2; result.m[0][2] = aiMat.a3; result.m[0][3] = aiMat.a4;
	result.m[1][0] = aiMat.b1; result.m[1][1] = aiMat.b2; result.m[1][2] = aiMat.b3; result.m[1][3] = aiMat.b4;
	result.m[2][0] = aiMat.c1; result.m[2][1] = aiMat.c2; result.m[2][2] = aiMat.c3; result.m[2][3] = aiMat.c4;
	result.m[3][0] = aiMat.d1; result.m[3][1] = aiMat.d2; result.m[3][2] = aiMat.d3; result.m[3][3] = aiMat.d4;

	// 資料の指示通りに転置(行列の向きを調整)
	return MatrixMath::Transpose(result);
}

void Model::Initialize(ModelManager* modelManager, const std::string& directoryPath, const std::string& filename) {
	modelManager_ = modelManager;
	DirectXCommon* dxCommon = modelManager_->GetDxCommon();

	LoadModelFile(directoryPath, filename);

	vertexResource_ = dxCommon->CreateBufferResource(sizeof(VertexData) * vertices_.size());
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = (UINT)(sizeof(VertexData) * vertices_.size());
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
	VertexData* vData = nullptr;
	vertexResource_->Map(0, nullptr, (void**)&vData);
	std::memcpy(vData, vertices_.data(), sizeof(VertexData) * vertices_.size());
	vertexResource_->Unmap(0, nullptr);

	if (!indices_.empty()) {
		indexCount_ = (UINT)indices_.size();
		indexResource_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * indices_.size());
		indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
		indexBufferView_.SizeInBytes = (UINT)(sizeof(uint32_t) * indices_.size());
		indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
		uint32_t* iData = nullptr;
		indexResource_->Map(0, nullptr, (void**)&iData);
		std::memcpy(iData, indices_.data(), sizeof(uint32_t) * indices_.size());
		indexResource_->Unmap(0, nullptr);
	}
}

/**
 * メモリ上のデータから直接初期化 (PrimitiveGenerator 用)
 */
void Model::InitializeWithData(ModelManager* modelManager, const std::vector<VertexData>& vertices, const std::vector<uint32_t>& indices) {
	modelManager_ = modelManager;
	vertices_ = vertices;
	indices_ = indices;

	// 生成モデルには階層がないため、RootNode を単位行列で初期化
	rootNode_.localMatrix = MatrixMath::MakeIdentity4x4();
	rootNode_.name = "GeneratedRoot";

	DirectXCommon* dxCommon = modelManager_->GetDxCommon();
	vertexResource_ = dxCommon->CreateBufferResource(sizeof(VertexData) * vertices_.size());
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = (UINT)(sizeof(VertexData) * vertices_.size());
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
	VertexData* vData = nullptr;
	vertexResource_->Map(0, nullptr, (void**)&vData);
	std::memcpy(vData, vertices_.data(), sizeof(VertexData) * vertices_.size());
	vertexResource_->Unmap(0, nullptr);

	if (!indices_.empty()) {
		indexCount_ = (UINT)indices_.size();
		indexResource_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * indices_.size());
		indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
		indexBufferView_.SizeInBytes = (UINT)(sizeof(uint32_t) * indices_.size());
		indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
		uint32_t* iData = nullptr;
		indexResource_->Map(0, nullptr, (void**)&iData);
		std::memcpy(iData, indices_.data(), sizeof(uint32_t) * indices_.size());
		indexResource_->Unmap(0, nullptr);
	}
}

Animation Model::LoadAnimation(const std::string& directoryPath, const std::string& filename) {
	Assimp::Importer importer;
	std::string filepath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filepath.c_str(), 0);
	assert(scene && scene->mNumAnimations > 0);

	aiAnimation* animAssimp = scene->mAnimations[0];
	Animation animation;
	animation.duration = (float)animAssimp->mDuration / (float)animAssimp->mTicksPerSecond;

	for (uint32_t i = 0; i < animAssimp->mNumChannels; ++i) {
		aiNodeAnim* nodeAnimAssimp = animAssimp->mChannels[i];
		NodeAnimation& nodeAnim = animation.nodeAnimations[nodeAnimAssimp->mNodeName.C_Str()];

		// Position (資料スライド 12)
		for (uint32_t k = 0; k < nodeAnimAssimp->mNumPositionKeys; ++k) {
			aiVectorKey& key = nodeAnimAssimp->mPositionKeys[k];
			nodeAnim.translate.push_back({ (float)key.mTime / (float)animAssimp->mTicksPerSecond, {-key.mValue.x, key.mValue.y, key.mValue.z} });
		}
		// Rotation (右手->左手変換込み)
		for (uint32_t k = 0; k < nodeAnimAssimp->mNumRotationKeys; ++k) {
			aiQuatKey& key = nodeAnimAssimp->mRotationKeys[k];
			nodeAnim.rotate.push_back({ (float)key.mTime / (float)animAssimp->mTicksPerSecond, {key.mValue.x, -key.mValue.y, -key.mValue.z, key.mValue.w} });
		}
		// Scale
		for (uint32_t k = 0; k < nodeAnimAssimp->mNumScalingKeys; ++k) {
			aiVectorKey& key = nodeAnimAssimp->mScalingKeys[k];
			nodeAnim.scale.push_back({ (float)key.mTime / (float)animAssimp->mTicksPerSecond, {key.mValue.x, key.mValue.y, key.mValue.z} });
		}
	}
	return animation;
}

void Model::LoadModelFile(const std::string& directoryPath, const std::string& filename) {
	Assimp::Importer importer;
	std::string filepath = directoryPath + "/" + filename;
	std::string modelFolder = std::filesystem::path(filepath).parent_path().string();

	const aiScene* scene = importer.ReadFile(filepath.c_str(),
		aiProcess_Triangulate | aiProcess_FlipWindingOrder | aiProcess_FlipUVs);

	if (!scene || !scene->HasMeshes()) {
		std::string errorStr = importer.GetErrorString();
		OutputDebugStringA(("Assimp Error: " + errorStr + "\n").c_str());
		assert(false && "Failed to load model file!");
	}

	// --- 追加：ルートノードからの階層解析 ---
	rootNode_ = ReadNode(scene->mRootNode);

	// 1. マテリアルの解析
	for (uint32_t i = 0; i < scene->mNumMaterials; ++i) {
		aiMaterial* material = scene->mMaterials[i];
		aiString name;
		material->Get(AI_MATKEY_NAME, name);
		ModelMaterialData& matData = modelMaterials_[name.C_Str()];
		if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
			aiString texturePath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath);
			matData.textureFilePath = modelFolder + "/" + texturePath.C_Str();
		}
	}

	// 2. メッシュの解析
	for (uint32_t i = 0; i < scene->mNumMeshes; ++i) {
		aiMesh* mesh = scene->mMeshes[i];
		Mesh currentMesh{};
		currentMesh.start = (uint32_t)indices_.size();
		currentMesh.count = mesh->mNumFaces * 3;
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		aiString matName;
		material->Get(AI_MATKEY_NAME, matName);
		currentMesh.materialName = matName.C_Str();

		uint32_t vertexBase = (uint32_t)vertices_.size();
		for (uint32_t v = 0; v < mesh->mNumVertices; ++v) {
			VertexData vertex{};
			vertex.position = { mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z, 1.0f };
			vertex.normal = { mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z };
			if (mesh->HasTextureCoords(0)) {
				vertex.texcoord = { mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y };
			}
			vertex.position.x *= -1.0f;
			vertex.normal.x *= -1.0f;
			vertices_.push_back(vertex);
		}
		for (uint32_t f = 0; f < mesh->mNumFaces; ++f) {
			aiFace& face = mesh->mFaces[f];
			for (uint32_t idx = 0; idx < face.mNumIndices; ++idx) {
				indices_.push_back(face.mIndices[idx] + vertexBase);
			}
		}
		meshes_.push_back(currentMesh);
	}
}

/**
 * 再帰的にノードを解析する関数 (資料スライド 5)
 */
Model::Node Model::ReadNode(aiNode* node) {
	Node result;

	// ノードの変形行列を取得して変換 (資料スライド 9)
	result.localMatrix = ConvertAiMatrix(node->mTransformation);

	// 名前を取得
	result.name = node->mName.C_Str();

	// 子供の数だけメモリを確保
	result.children.resize(node->mNumChildren);

	// 子供たちに対しても同じ処理を呼び出す (再帰)
	for (uint32_t i = 0; i < node->mNumChildren; ++i) {
		result.children[i] = ReadNode(node->mChildren[i]);
	}

	return result;
}

/**
 * テクスチャの一括読み込み
 */
void Model::LoadTextures(SpriteCommon* spriteCommon) {
	for (auto& pair : modelMaterials_) {
		if (!pair.second.textureFilePath.empty()) {
			pair.second.textureHandle = spriteCommon->LoadTexture(pair.second.textureFilePath);
		}
	}
}

/**
 * 描画
 */
void Model::Draw(DirectXCommon* dxCommon) {
	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetIndexBuffer(&indexBufferView_);

	if (meshes_.empty()) {
		commandList->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);
	}
	else {
		for (const auto& mesh : meshes_) {
			auto it = modelMaterials_.find(mesh.materialName);
			if (it != modelMaterials_.end()) {
				D3D12_GPU_DESCRIPTOR_HANDLE handle = modelManager_->GetSrvManager()->GetGPUDescriptorHandle(it->second.textureHandle);
				commandList->SetGraphicsRootDescriptorTable(5, handle);
			}
			commandList->DrawIndexedInstanced(mesh.count, 1, mesh.start, 0, 0);
		}
	}
}

/**
 * ★追加：インスタンシング描画
 */
void Model::Draw(ID3D12GraphicsCommandList* commandList, uint32_t instanceCount) {
	// 頂点バッファとインデックスバッファをセット
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetIndexBuffer(&indexBufferView_);

	if (meshes_.empty()) {
		// メッシュ情報がない場合は全体をそのまま描画
		commandList->DrawIndexedInstanced(indexCount_, instanceCount, 0, 0, 0);
	}
	else {
		for (const auto& mesh : meshes_) {
			// パーティクルシステム側でテクスチャを管理しているため、
			// ここではメッシュごとのテクスチャ切り替えは行わず、指定された数だけ描画します。
			commandList->DrawIndexedInstanced(mesh.count, instanceCount, mesh.start, 0, 0);
		}
	}
}