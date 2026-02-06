#include "Model.h"
#include "ModelCommon.h" // ModelCommonを使うためインクルード
#include "Logger.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>

// 引数を ModelCommon* に変更
void Model::Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename) {
    modelCommon_ = modelCommon;
    // ModelCommon から DirectXCommon をもらう
    DirectXCommon* dxCommon = modelCommon_->GetDxCommon();

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

// 引数を DirectXCommon* に変更
void Model::Draw(DirectXCommon* dxCommon) {
    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->DrawInstanced(UINT(vertices_.size()), 1, 0, 0);
}

// ... LoadObjFile と LoadMaterialTemplateFile は以前と同じ内容なので省略せずに記述 ...
// (ファイル読み込み部分は以前の実装のままです)

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
        } else if (identifier == "mtllib") {
            std::string materialFilename;
            s >> materialFilename;
            LoadMaterialTemplateFile(directoryPath, materialFilename);
        }
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
    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier;
        if (identifier == "map_Kd") {
            std::string textureFilename;
            s >> textureFilename;
            materialData_.textureFilePath = directoryPath + "/" + textureFilename;
        }
    }
}