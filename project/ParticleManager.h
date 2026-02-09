#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "Matrix.h"
#include "Transform.h"

// 前方宣言
class DirectXCommon;
class SrvManager;
class Camera;

// パーティクル1粒のデータ
struct Particle {
    Vector3 position;    // 位置
    Vector3 velocity;    // 速度
    Vector3 acceleration;// 加速度
    Vector4 color;       // 色
    float lifeTime;      // 寿命（最大時間）
    float currentTime;   // 現在の経過時間
    float size;          // スケール
    float startSize;     // 開始時のサイズ
    float endSize;       // 終了時のサイズ
};

// パーティクルグループ
struct ParticleGroup {
    std::string name;          // グループ名
    std::list<Particle> particles; // パーティクルのリスト
    uint32_t textureHandle;    // テクスチャハンドル
};

class ParticleManager {
public:
    // 初期化
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    // 更新
    void Update(Camera* camera);

    // 描画
    void Draw();

    // パーティクルグループの作成
    // textureHandle: SpriteCommonなどでロードしたテクスチャのハンドルを指定
    // ★ここを修正: 第2引数を uint32_t に統一
    void CreateParticleGroup(const std::string& name, uint32_t textureHandle);

    // パーティクルの発生
    void Emit(const std::string& name, const Vector3& position, uint32_t count);

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Camera* camera_ = nullptr;

    // ルートシグネチャとパイプラインステート
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    // 頂点バッファ（矩形ポリゴン用）
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    // インスタンシング用バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
    D3D12_VERTEX_BUFFER_VIEW instancingBufferView_{};

    struct InstancingData {
        Matrix4x4 WVP;  // ワールド x ビュー x プロジェクション
        Matrix4x4 World; // ワールド行列
        Vector4 color;  // 色
    };
    InstancingData* instancingData_ = nullptr;

    // 最大パーティクル数
    static const uint32_t kMaxInstanceCount = 1024;

    // パーティクルグループのコンテナ
    std::map<std::string, ParticleGroup> particleGroups_;

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();
    void CreateModel();
};