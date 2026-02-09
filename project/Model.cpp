#include "Model.h"
#include "ModelManager.h"
#include "SrvManager.h" // ★追加
#include "SpriteCommon.h" 
#include "Logger.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>

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

void Model::LoadTextures(SpriteCommon* spriteCommon) {
    for (auto& entry : modelMaterials_) {
        const std::string& path = entry.second.textureFilePath;
        if (!path.empty()) {
            entry.second.textureHandle = spriteCommon->LoadTexture(path);
        }
    }
    if (!modelMaterials_.empty()) {
        materialData_.textureFilePath = modelMaterials_.begin()->second.textureFilePath;
    }
}

void Model::Draw(DirectXCommon* dxCommon) {
    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    if (meshes_.empty()) {
        commandList->DrawInstanced(UINT(vertices_.size()), 1, 0, 0);
        return;
    }

    // ★重要: ModelManager経由でSrvManagerを取得する
    SrvManager* srvManager = modelManager_->GetSrvManager();

    for (const auto& mesh : meshes_) {
        if (modelMaterials_.contains(mesh.materialName)) {
            uint32_t handle = modelMaterials_[mesh.materialName].textureHandle;
            if (handle != 0) {
                // ★修正: SrvManagerからハンドルを取得
                D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = srvManager->GetGPUDescriptorHandle(handle);
                commandList->SetGraphicsRootDescriptorTable(3, srvHandle);
            }
        }
        commandList->DrawInstanced(mesh.count, 1, mesh.start, 0);
    }
}

// ... LoadObjFileとLoadMaterialTemplateFileは変更なしのため省略 (ファイルには残してください) ...
// (前のコードと同じです)

void Model::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
    std::string filepath = directoryPath + "/" + filename;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        Logger::Log("Failed to open OBJ file: " + filepath);
        assert(false);
    }

    std::vector<Vector4> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::string line;

    Mesh currentMesh = {};
    currentMesh.start = 0;
    currentMesh.count = 0;
    currentMesh.materialName = "";

    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier;

        if (identifier == "v") {
            Vector4 position;
            s >> position.x >> position.y >> position.z;
            position.w = 1.0f;
            position.x *= -1.0f;
            positions.push_back(position);
        } else if (identifier == "vt") {
            Vector2 texcoord;
            s >> texcoord.x >> texcoord.y;
            texcoord.y = 1.0f - texcoord.y;
            texcoords.push_back(texcoord);
        } else if (identifier == "vn") {
            Vector3 normal;
            s >> normal.x >> normal.y >> normal.z;
            normal.x *= -1.0f;
            normals.push_back(normal);
        } else if (identifier == "f") {
            VertexData triangle[3];
            for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
                std::string vertexDefinition;
                s >> vertexDefinition;
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
        } else if (identifier == "mtllib") {
            std::string materialFilename;
            s >> materialFilename;
            LoadMaterialTemplateFile(directoryPath, materialFilename);
        } else if (identifier == "usemtl") {
            std::string materialName;
            s >> materialName;
            if (currentMesh.count > 0) {
                meshes_.push_back(currentMesh);
            }
            currentMesh.start = UINT(vertices_.size());
            currentMesh.count = 0;
            currentMesh.materialName = materialName;
        }
    }
    if (currentMesh.count > 0) {
        meshes_.push_back(currentMesh);
    }
}

void Model::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
    std::string filepath = directoryPath + "/" + filename;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        Logger::Log("Failed to open MTL file: " + filepath);
        assert(false);
    }
    std::string line;
    std::string currentMaterialName;
    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier;
        if (identifier == "newmtl") {
            s >> currentMaterialName;
        } else if (identifier == "map_Kd") {
            std::string textureFilename;
            s >> textureFilename;
            modelMaterials_[currentMaterialName].textureFilePath = directoryPath + "/" + textureFilename;
        }
    }
}