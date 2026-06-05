#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "math/Matrix.h"
#include "math/Transform.h"

// 前方宣言
class DirectXCommon;
class SrvManager;
class Camera;
class Model;

/*
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

    //UVスクロール用パラメータ
    Vector2 uvScale = { 1.0f,1.0f };
    Vector2 uvOffset = { 0.0f,0.0f };
	Vector2 uvVelocity = { 0.0f,0.0f };
};

/**
 * ParticleGroup 構造体
 */
struct ParticleGroup {
    std::string name;
    std::list<Particle> particles;
    uint32_t textureHandle;
	Model* model = nullptr;
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
    void CreateParticleGroup(const std::string& name, uint32_t textureHandle, Model* model = nullptr);

    // ★新機能：パーティクルを1つ追加して、その参照を返す
    // 火花など、1粒ずつのパラメータを細かく設定したい時用
    Particle& AddParticle(const std::string& name, const Vector3& position);

    // ★従来機能：指定した数だけランダムに放出する
    // 爆発や煙など、大量に出したい時用
    void Emit(const std::string& name, const Vector3& position, uint32_t count);

    //全パーティクル削除
    void ClearAll();

    // discardしきい値の設定（資料スライド10対応）
    // この値以下のアルファ値を持つピクセルが棄却（discard）される
    void SetAlphaReference(float value) { alphaReference_ = value; }
    float GetAlphaReference() const { return alphaReference_; }

    // 指定グループのパーティクル数を取得（常時表示判定に使用）
    uint32_t GetParticleCount(const std::string& name) const {
        auto it = particleGroups_.find(name);
        if (it == particleGroups_.end()) return 0;
        return static_cast<uint32_t>(it->second.particles.size());
    }
private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();
    void CreateModel();
    void CreateGPUParticleResources();
    void CreateGPUParticleComputeRootSignature();
    void CreateGPUParticleComputePipelineState();
    void CreateGPUParticleGraphicsRootSignature();
    void CreateGPUParticleGraphicsPipelineState();
    void InitializeGPUParticles();
    void DrawGPUParticles();
    bool TryGetGPUParticleTextureHandle(uint32_t& textureHandle) const;

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Camera* camera_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> gpuParticleComputeRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gpuParticleComputePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> gpuParticleGraphicsRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gpuParticleGraphicsPipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
    D3D12_VERTEX_BUFFER_VIEW instancingBufferView_{};

    struct InstancingData {
        Matrix4x4 WVP;
        Matrix4x4 World;
        Vector4 color;
		Vector4 uvTransform; // x,y:Scale、z,w:Offset
    };
    InstancingData* instancingData_ = nullptr;

    struct ParticleCS {
        Vector3 translate;
        Vector3 scale;
        float lifeTime;
        Vector3 velocity;
        float currentTime;
        Vector4 color;
    };
    static_assert(sizeof(ParticleCS) == 60, "ParticleCS layout must match HLSL.");

    struct GPUParticleViewData {
        Matrix4x4 viewProjection;
        Matrix4x4 billboardMatrix;
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> gpuParticleResource_;
    uint32_t gpuParticleSrvHandle_ = UINT32_MAX;
    uint32_t gpuParticleUavHandle_ = UINT32_MAX;
    D3D12_RESOURCE_STATES gpuParticleResourceState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

    Microsoft::WRL::ComPtr<ID3D12Resource> gpuParticleViewResource_;
    GPUParticleViewData* gpuParticleViewData_ = nullptr;
    bool gpuParticlesInitialized_ = false;

    // discardしきい値用の定数バッファ
    // GPU側のピクセルシェーダーに渡すためのバッファ
    struct MaterialData {
        float alphaReference; // この値以下のα値はdiscardされる
        float padding[3];     // CBufferは16バイトアラインメントが必要
    };
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    MaterialData* materialData_ = nullptr;
    float alphaReference_ = 0.0f; // CPU側の設定値

    static const uint32_t kMaxInstanceCount = 1024;
    static constexpr uint32_t kMaxGPUParticleCount = 1024;
    std::map<std::string, ParticleGroup> particleGroups_;

};
