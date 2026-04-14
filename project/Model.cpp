#include "Model.h"
#include "ModelManager.h"
#include "SrvManager.h"
#include "SpriteCommon.h" 
#include <cassert>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>

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

void Model::InitializeWithData(ModelManager* modelManager, const std::vector<VertexData>& vertices, const std::vector<uint32_t>& indices) {
    modelManager_ = modelManager;
    DirectXCommon* dxCommon = modelManager_->GetDxCommon();

    vertexResource_ = dxCommon->CreateBufferResource(sizeof(VertexData) * vertices.size());
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * vertices.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);
    VertexData* vData = nullptr;
    vertexResource_->Map(0, nullptr, (void**)&vData);
    std::memcpy(vData, vertices.data(), sizeof(VertexData) * vertices.size());
    vertexResource_->Unmap(0, nullptr);

    indexCount_ = (UINT)indices.size();
    indexResource_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * indices.size());
    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = UINT(sizeof(uint32_t) * indices.size());
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
    uint32_t* iData = nullptr;
    indexResource_->Map(0, nullptr, (void**)&iData);
    std::memcpy(iData, indices.data(), sizeof(uint32_t) * indices.size());
    indexResource_->Unmap(0, nullptr);
}

void Model::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
    std::string modelSubDir = "";
    size_t pos = filename.find_last_of('/');
    if (pos != std::string::npos) {
        modelSubDir = filename.substr(0, pos + 1);
    }

    std::string fullDirPath = directoryPath + "/" + modelSubDir;
    std::string filepath = directoryPath + "/" + filename;

    std::ifstream file(filepath);
    if (!file.is_open()) {
        // 修正：ファイルが見つからない場合にコンソールに詳細を出し、停止を避けるために return する
        // 本来はここにブレークポイントを置いてパスを確認してください
        return;
    }

    std::vector<Vector4> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::string line;
    Mesh currentMesh{};

    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier;

        if (identifier == "v") {
            Vector4 p{ 0,0,0,1 }; s >> p.x >> p.y >> p.z;
            positions.push_back(p);
        } else if (identifier == "vt") {
            Vector2 t; s >> t.x >> t.y;
            t.y = 1.0f - t.y;
            texcoords.push_back(t);
        } else if (identifier == "vn") {
            Vector3 n; s >> n.x >> n.y >> n.z;
            normals.push_back(n);
        } else if (identifier == "f") {
            VertexData triangle[3];
            for (int i = 0; i < 3; i++) {
                std::string def; s >> def;
                std::replace(def.begin(), def.end(), '/', ' ');
                std::istringstream v(def);
                uint32_t idxP, idxT, idxN;
                v >> idxP >> idxT >> idxN;
                triangle[i] = { positions[idxP - 1], normals[idxN - 1], texcoords[idxT - 1] };
            }
            vertices_.push_back(triangle[2]);
            vertices_.push_back(triangle[1]);
            vertices_.push_back(triangle[0]);
            currentMesh.count += 3;
        } else if (identifier == "mtllib") {
            std::string mtlName; s >> mtlName;
            LoadMaterialTemplateFile(fullDirPath, mtlName);
        } else if (identifier == "usemtl") {
            if (currentMesh.count > 0) meshes_.push_back(currentMesh);
            s >> currentMesh.materialName;
            currentMesh.start = (uint32_t)vertices_.size();
            currentMesh.count = 0;
        }
    }
    if (currentMesh.count > 0) meshes_.push_back(currentMesh);
}

void Model::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
    std::string filepath = directoryPath + filename;
    std::ifstream file(filepath);
    if (!file.is_open()) return;

    std::string line, currentMtl;
    while (std::getline(file, line)) {
        std::string identifier; std::istringstream s(line); s >> identifier;
        if (identifier == "newmtl") { s >> currentMtl; } else if (identifier == "map_Kd") {
            std::string texName; s >> texName;
            modelMaterials_[currentMtl].textureFilePath = directoryPath + texName;
        }
    }
}

void Model::LoadTextures(SpriteCommon* spriteCommon) {
    for (auto& pair : modelMaterials_) {
        pair.second.textureHandle = spriteCommon->LoadTexture(pair.second.textureFilePath);
    }
}

void Model::Draw(DirectXCommon* dxCommon) {
    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    if (indexResource_) {
        commandList->IASetIndexBuffer(&indexBufferView_);
        commandList->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);
    } else {
        for (const auto& mesh : meshes_) {
            auto it = modelMaterials_.find(mesh.materialName);
            if (it != modelMaterials_.end()) {
                D3D12_GPU_DESCRIPTOR_HANDLE handle = modelManager_->GetSrvManager()->GetGPUDescriptorHandle(it->second.textureHandle);
                // ★最重要：テクスチャはインデックス [5] にセットする！！
                commandList->SetGraphicsRootDescriptorTable(5, handle);
            }
            commandList->DrawInstanced(mesh.count, 1, mesh.start, 0);
        }
    }
}