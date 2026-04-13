#include "Model.h"
#include "ModelManager.h"
#include "SrvManager.h"
#include "SpriteCommon.h" 
#include "Logger.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>

/**
 * ファイルからの初期化
 */
void Model::Initialize(ModelManager* modelManager, const std::string& directoryPath, const std::string& filename) {
    modelManager_ = modelManager;
    DirectXCommon* dxCommon = modelManager_->GetDxCommon();

    LoadObjFile(directoryPath, filename);

    vertexResource_ = dxCommon->CreateBufferResource(sizeof(VertexData) * vertices_.size());

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * vertices_.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    VertexData* vertexData = nullptr;
    vertexResource_->Map(0, nullptr, (void**)&vertexData);
    std::memcpy(vertexData, vertices_.data(), sizeof(VertexData) * vertices_.size());
    vertexResource_->Unmap(0, nullptr);
}

/**
 * メモリ上のデータから初期化 (PrimitiveGenerator 用)
 */
void Model::InitializeWithData(ModelManager* modelManager, const std::vector<VertexData>& vertices, const std::vector<uint32_t>& indices) {
    modelManager_ = modelManager;
    DirectXCommon* dxCommon = modelManager_->GetDxCommon();
    vertices_ = vertices;

    // 1. 頂点バッファの作成
    vertexResource_ = dxCommon->CreateBufferResource(sizeof(VertexData) * vertices_.size());
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * vertices_.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    VertexData* vertexData = nullptr;
    vertexResource_->Map(0, nullptr, (void**)&vertexData);
    std::memcpy(vertexData, vertices_.data(), sizeof(VertexData) * vertices_.size());
    vertexResource_->Unmap(0, nullptr);

    // 2. インデックスバッファの作成
    if (!indices.empty()) {
        indexCount_ = UINT(indices.size());
        indexResource_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * indexCount_);

        indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
        indexBufferView_.SizeInBytes = sizeof(uint32_t) * indexCount_;
        indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

        uint32_t* indexData = nullptr;
        indexResource_->Map(0, nullptr, (void**)&indexData);
        std::memcpy(indexData, indices.data(), sizeof(uint32_t) * indexCount_);
        indexResource_->Unmap(0, nullptr);
    }

    // 3. 描画用メッシュ情報の作成（デフォルト）
    Mesh mesh = {};
    mesh.start = 0;
    mesh.count = (indexCount_ > 0) ? indexCount_ : UINT(vertices_.size());
    mesh.materialName = "default";
    meshes_.push_back(mesh);

    modelMaterials_["default"] = { "", 0 };
}

void Model::LoadTextures(SpriteCommon* spriteCommon) {
    for (auto& entry : modelMaterials_) {
        const std::string& path = entry.second.textureFilePath;
        if (!path.empty()) { entry.second.textureHandle = spriteCommon->LoadTexture(path); }
    }
}

/**
 * 描画
 */
void Model::Draw(DirectXCommon* dxCommon) {
    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

    // 頂点バッファのセット
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // インデックスバッファがあればセット
    if (indexCount_ > 0) {
        commandList->IASetIndexBuffer(&indexBufferView_);
    }

    SrvManager* srvManager = modelManager_->GetSrvManager();

    for (const auto& mesh : meshes_) {
        if (modelMaterials_.contains(mesh.materialName)) {
            uint32_t handle = modelMaterials_[mesh.materialName].textureHandle;
            if (handle != 0) {
                D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = srvManager->GetGPUDescriptorHandle(handle);
                commandList->SetGraphicsRootDescriptorTable(3, srvHandle);
            }
        }

        // インデックスの有無で描画命令を切り替える
        if (indexCount_ > 0) {
            commandList->DrawIndexedInstanced(mesh.count, 1, mesh.start, 0, 0);
        } else {
            commandList->DrawInstanced(mesh.count, 1, mesh.start, 0);
        }
    }
}

// OBJ/MTL読み込み処理 (メンバー指定代入なので、メンバの順序入れ替え後もそのまま動作します)
void Model::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
    std::string filepath = directoryPath + "/" + filename;
    std::ifstream file(filepath);
    if (!file.is_open()) { assert(false); }

    std::vector<Vector4> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::string line;

    Mesh currentMesh = { 0, 0, "" };

    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier;

        if (identifier == "v") {
            Vector4 position; s >> position.x >> position.y >> position.z;
            position.w = 1.0f; position.x *= -1.0f;
            positions.push_back(position);
        } else if (identifier == "vt") {
            Vector2 texcoord; s >> texcoord.x >> texcoord.y;
            texcoord.y = 1.0f - texcoord.y;
            texcoords.push_back(texcoord);
        } else if (identifier == "vn") {
            Vector3 normal; s >> normal.x >> normal.y >> normal.z;
            normal.x *= -1.0f;
            normals.push_back(normal);
        } else if (identifier == "f") {
            VertexData triangle[3];
            for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
                std::string vertexDefinition; s >> vertexDefinition;
                std::istringstream v(vertexDefinition);
                std::string indexStr;
                int32_t indices[3] = { 0, 0, 0 };
                int32_t i = 0;
                while (std::getline(v, indexStr, '/')) {
                    if (!indexStr.empty()) indices[i] = std::stoi(indexStr);
                    i++;
                }
                triangle[faceVertex].position = positions[indices[0] - 1];
                triangle[faceVertex].texcoord = texcoords[indices[1] - 1];
                triangle[faceVertex].normal = normals[indices[2] - 1];
            }
            vertices_.push_back(triangle[2]);
            vertices_.push_back(triangle[1]);
            vertices_.push_back(triangle[0]);
            currentMesh.count += 3;
        } else if (identifier == "usemtl") {
            s >> currentMesh.materialName;
            currentMesh.start = UINT(vertices_.size());
            meshes_.push_back(currentMesh);
        }
    }
}

void Model::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
    std::string filepath = directoryPath + "/" + filename;
    std::ifstream file(filepath);
    if (!file.is_open()) { assert(false); }
    std::string line, currentMaterialName;
    while (std::getline(file, line)) {
        std::string identifier; std::istringstream s(line); s >> identifier;
        if (identifier == "newmtl") { s >> currentMaterialName; } else if (identifier == "map_Kd") {
            std::string texName; s >> texName;
            modelMaterials_[currentMaterialName].textureFilePath = directoryPath + "/" + texName;
        }
    }
}