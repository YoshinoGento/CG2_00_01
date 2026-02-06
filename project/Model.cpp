#include "Model.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>

void Model::Initialize(Object3dCommon* object3dCommon, const std::string& directoryPath, const std::string& filename) {
    object3dCommon_ = object3dCommon;
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();

    // 1. ファイルからデータを読み込む
    LoadObjFile(directoryPath, filename);

    // 2. 頂点バッファの作成
    // 読み込んだ頂点数分のサイズを確保する
    vertexResource_ = dxCommon->CreateBufferResource(sizeof(VertexData) * vertices_.size());

    // 3. ビューの設定
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * vertices_.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    // 4. 頂点データの書き込み
    VertexData* vertexData = nullptr;
    vertexResource_->Map(0, nullptr, (void**)&vertexData);
    // std::memcpy で一気にコピー
    std::memcpy(vertexData, vertices_.data(), sizeof(VertexData) * vertices_.size());
    vertexResource_->Unmap(0, nullptr);
}

void Model::Draw(Object3dCommon* object3dCommon) {
    ID3D12GraphicsCommandList* commandList = object3dCommon->GetDxCommon()->GetCommandList();

    // 頂点バッファをセット
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // 描画 (読み込んだ頂点数分だけ描画)
    commandList->DrawInstanced(UINT(vertices_.size()), 1, 0, 0);
}

// OBJファイル読み込みの実装
void Model::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
    std::string filepath = directoryPath + "/" + filename;
    std::ifstream file(filepath);
    assert(file.is_open()); // ファイルが開けなかったら止まる

    // 一時的にデータを蓄える場所
    std::vector<Vector4> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::string line;

    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier; // 先頭の文字を取得 (v, vt, vn, f など)

        // 頂点位置 (v)
        if (identifier == "v") {
            Vector4 position;
            s >> position.x >> position.y >> position.z;
            position.w = 1.0f;
            // X軸を反転 (Blender等の座標系合わせ)
            position.x *= -1.0f;
            positions.push_back(position);
        }
        // テクスチャ座標 (vt)
        else if (identifier == "vt") {
            Vector2 texcoord;
            s >> texcoord.x >> texcoord.y;
            // Y軸を反転 (DirectXは左上が0,0)
            texcoord.y = 1.0f - texcoord.y;
            texcoords.push_back(texcoord);
        }
        // 法線 (vn)
        else if (identifier == "vn") {
            Vector3 normal;
            s >> normal.x >> normal.y >> normal.z;
            // X軸を反転
            normal.x *= -1.0f;
            normals.push_back(normal);
        }
        // 面 (f)
        else if (identifier == "f") {
            // f 頂点/UV/法線 頂点/UV/法線 ... という形式
            // 今回は三角形分割済みのデータを想定 (頂点3つ)
            VertexData triangle[3];

            for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
                std::string vertexDefinition;
                s >> vertexDefinition;

                std::istringstream v(vertexDefinition);
                std::string indexStr;
                int32_t indices[3] = { 0, 0, 0 }; // 位置, UV, 法線
                int32_t i = 0;

                // 文字列を / で区切ってインデックスを取り出す
                while (std::getline(v, indexStr, '/')) {
                    if (!indexStr.empty()) {
                        indices[i] = std::stoi(indexStr);
                    }
                    i++;
                }

                // OBJのインデックスは1始まりなので、-1して0始まりにする
                // 配列からデータを取り出して構築
                triangle[faceVertex].position = positions[indices[0] - 1];
                triangle[faceVertex].texcoord = texcoords[indices[1] - 1];
                triangle[faceVertex].normal = normals[indices[2] - 1];
            }

            // 頂点データを登録 (三角形の順序を逆にしてカリング対策)
            vertices_.push_back(triangle[2]);
            vertices_.push_back(triangle[1]);
            vertices_.push_back(triangle[0]);
        }
    }
}