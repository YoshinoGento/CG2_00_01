#include "Model.h"
#include "ModelManager.h"
#include "SrvManager.h"
#include "SpriteCommon.h" 
#include <cassert>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <filesystem> // パス操作用

/**
 * ファイルからの初期化
 */
void Model::Initialize(ModelManager* modelManager, const std::string& directoryPath, const std::string& filename) {
    modelManager_ = modelManager;
    DirectXCommon* dxCommon = modelManager_->GetDxCommon();

    // 1. Assimpでファイルをロード（解析）
    LoadModelFile(directoryPath, filename);

    // 2. 頂点バッファの作成
    vertexResource_ = dxCommon->CreateBufferResource(sizeof(VertexData) * vertices_.size());
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * vertices_.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    VertexData* vertexData = nullptr;
    vertexResource_->Map(0, nullptr, (void**)&vertexData);
    std::memcpy(vertexData, vertices_.data(), sizeof(VertexData) * vertices_.size());
    vertexResource_->Unmap(0, nullptr);

    // 3. インデックスバッファの作成
    if (!indices_.empty()) {
        indexCount_ = (UINT)indices_.size();
        indexResource_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * indices_.size());
        indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
        indexBufferView_.SizeInBytes = UINT(sizeof(uint32_t) * indices_.size());
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

    DirectXCommon* dxCommon = modelManager_->GetDxCommon();

    vertexResource_ = dxCommon->CreateBufferResource(sizeof(VertexData) * vertices.size());
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * vertices.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    VertexData* vertexData = nullptr;
    vertexResource_->Map(0, nullptr, (void**)&vertexData);
    std::memcpy(vertexData, vertices_.data(), sizeof(VertexData) * vertices_.size());
    vertexResource_->Unmap(0, nullptr);

    if (!indices_.empty()) {
        indexCount_ = (UINT)indices_.size();
        indexResource_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * indices_.size());
        indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
        indexBufferView_.SizeInBytes = UINT(sizeof(uint32_t) * indices_.size());
        indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

        uint32_t* iData = nullptr;
        indexResource_->Map(0, nullptr, (void**)&iData);
        std::memcpy(iData, indices_.data(), sizeof(uint32_t) * indices_.size());
        indexResource_->Unmap(0, nullptr);
    }
}

/**
 * Assimp によるファイル解析
 */
void Model::LoadModelFile(const std::string& directoryPath, const std::string& filename) {
    Assimp::Importer importer;
    std::string filepath = directoryPath + "/" + filename;

    // モデルファイルがあるフォルダを特定する（テクスチャを探すため）
    std::string modelFolder = std::filesystem::path(filepath).parent_path().string();

    const aiScene* scene = importer.ReadFile(filepath.c_str(),
        aiProcess_Triangulate | aiProcess_FlipWindingOrder | aiProcess_FlipUVs);

    if (!scene || !scene->HasMeshes()) {
        std::string errorStr = importer.GetErrorString();
        OutputDebugStringA(("Assimp Error: " + errorStr + "\n").c_str());
        assert(false && "Failed to load model file!");
    }

    // 1. マテリアルの解析
    for (uint32_t i = 0; i < scene->mNumMaterials; ++i) {
        aiMaterial* material = scene->mMaterials[i];
        aiString name;
        material->Get(AI_MATKEY_NAME, name);
        ModelMaterialData& matData = modelMaterials_[name.C_Str()];

        if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
            aiString texturePath;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath);
            // ★重要：モデルファイルと同じフォルダから画像を探す
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
 * テクスチャの一括読み込み
 */
void Model::LoadTextures(SpriteCommon* spriteCommon) {
    for (auto& pair : modelMaterials_) {
        if (!pair.second.textureFilePath.empty()) {
            pair.second.textureHandle = spriteCommon->LoadTexture(pair.second.textureFilePath);
            // デバッグログ：読み込んだパスを表示
            OutputDebugStringA(("Model Texture Loaded: " + pair.second.textureFilePath + "\n").c_str());
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
                // 5番スロットにテクスチャをセット
                commandList->SetGraphicsRootDescriptorTable(5, handle);
            }
            commandList->DrawIndexedInstanced(mesh.count, 1, mesh.start, 0, 0);
        }
    }
}