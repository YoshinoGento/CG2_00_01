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

/**
 * Particle 構造体
 * 従来の size 変数も活かしつつ、Transform を持たせることで
 * 「p.transform.scale」という書き方と、従来の「Emit」の両方に対応させます。
 */
struct Particle {
    Transform transform; // 座標、回転、スケールを一括管理
    Vector3 velocity;    // 速度
    Vector3 acceleration;// 加速度
    Vector4 color;       // 色
    float lifeTime;      // 寿命
    float currentTime;   // 経過時間

    // 従来の Emit 用のサイズ補間用変数
    float startSize = 1.0f;
    float endSize = 1.0f;
};

/**
 * ParticleGroup 構造体
 */
struct ParticleGroup {
    std::string name;
    std::list<Particle> particles;
    uint32_t textureHandle;
};

/**
 * ParticleManager クラス
 * 従来の Emit と、新しい AddParticle を両方搭載したハイブリッド版
 */
class ParticleManager {
public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void Update(Camera* camera);
    void Draw();

    // グループ作成
    void CreateParticleGroup(const std::string& name, uint32_t textureHandle);

    // ★新機能：パーティクルを1つ追加して、その参照を返す
    // 火花など、1粒ずつのパラメータを細かく設定したい時用
    Particle& AddParticle(const std::string& name, const Vector3& position);

    // ★従来機能：指定した数だけランダムに放出する
    // 爆発や煙など、大量に出したい時用
    void Emit(const std::string& name, const Vector3& position, uint32_t count);

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Camera* camera_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
    D3D12_VERTEX_BUFFER_VIEW instancingBufferView_{};

    struct InstancingData {
        Matrix4x4 WVP;
        Matrix4x4 World;
        Vector4 color;
    };
    InstancingData* instancingData_ = nullptr;

    static const uint32_t kMaxInstanceCount = 1024;
    std::map<std::string, ParticleGroup> particleGroups_;

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();
    void CreateModel();
};