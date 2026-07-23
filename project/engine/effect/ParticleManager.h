#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "math/Matrix.h"
#include "math/Transform.h"
#include "2d/TextureManager.h"

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
    Texture2DHandle textureHandle{};
	Model* model = nullptr;
};

/**
 * ParticleManager クラス
 * 従来の Emit と、新しい AddParticle を両方搭載したハイブリッド版
 */
struct GPUParticleEmitSettings {
    Vector3 translate;
    float radius;

    Vector4 color;

    Vector3 scale;
    float lifeTime;

    Vector3 baseVelocity;
    float speed;

    uint32_t count;
    uint32_t emit;
    uint32_t preset;
    uint32_t padding;
};
static_assert(sizeof(GPUParticleEmitSettings) == 80, "GPUParticleEmitSettings layout must match HLSL.");

enum class InteractionBrushOperation : uint32_t {
    None = 0,
    Push = 1,
    Pull = 2,
};

struct GPUParticleInteractionSettings {
    Vector3 gridCenter;
    float particleSize;

    uint32_t gridCountX;
    uint32_t gridCountY;
    uint32_t gridCountZ;
    uint32_t particleCount;

    Vector3 brushPosition;
    float brushRadius;

    float brushStrength;
    uint32_t isPressed;
    uint32_t operation;
    float deltaTime;

    float damping;
    float padding0;
    float padding1;
    float padding2;
};
static_assert(sizeof(GPUParticleInteractionSettings) == 80, "GPUParticleInteractionSettings layout must match HLSL.");
static_assert(offsetof(GPUParticleInteractionSettings, brushPosition) == 32, "GPUParticleInteractionSettings::brushPosition offset must match HLSL.");
static_assert(offsetof(GPUParticleInteractionSettings, operation) == 56, "GPUParticleInteractionSettings::operation offset must match HLSL.");
static_assert(offsetof(GPUParticleInteractionSettings, deltaTime) == 60, "GPUParticleInteractionSettings::deltaTime offset must match HLSL.");
static_assert(offsetof(GPUParticleInteractionSettings, damping) == 64, "GPUParticleInteractionSettings::damping offset must match HLSL.");

// Owns CPU particle groups and the shared GPU buffers, pipelines, and states.
// DirectXCommon, SrvManager, Camera, and group Models are non-owning dependencies.
class ParticleManager {
public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Update(Camera* camera, float deltaTime);
    void Draw(bool drawDefaultGPUParticles = true);
    void InitializeGPUParticleInteraction(const GPUParticleInteractionSettings& settings);
    void UpdateGPUParticleInteraction(const GPUParticleInteractionSettings& settings);
    void DrawGPUParticleBuffer();
    void RequestGPUParticleEmit(const Vector3& position, uint32_t count);
    void RequestGPUParticleEmit(const GPUParticleEmitSettings& settings);
    void ResetGPUParticles();

    // グループ作成
    void CreateParticleGroup(const std::string& name, Texture2DHandle textureHandle, Model* model = nullptr);

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
    void CreateGPUParticleEmitComputeRootSignature();
    void CreateGPUParticleEmitComputePipelineState();
    void CreateGPUParticleUpdateComputeRootSignature();
    void CreateGPUParticleUpdateComputePipelineState();
    void CreateGPUParticleInteractionComputeRootSignature();
    void CreateGPUParticleInteractionComputePipelineStates();
    void CreateGPUParticleGraphicsRootSignature();
    void CreateGPUParticleGraphicsPipelineState();
    void InitializeGPUParticles();
    void EmitGPUParticles();
    void UpdateGPUParticles();
    void DrawGPUParticles();
    bool TryGetGPUParticleTextureHandle(Texture2DHandle& textureHandle) const;

private:
    // Non-owning services and current-frame context.
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Camera* camera_ = nullptr;
	float frameDeltaTime_ = 1.0f / 60.0f;

    // CPU and GPU pipeline state owned by this manager.
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> gpuParticleComputeRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gpuParticleComputePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> gpuParticleEmitComputeRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gpuParticleEmitComputePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> gpuParticleUpdateComputeRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gpuParticleUpdateComputePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> gpuParticleInteractionComputeRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gpuParticleInteractionInitializeComputePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gpuParticleInteractionUpdateComputePipelineState_;
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
        uint32_t isAlive;
    };
    static_assert(sizeof(Vector3) == 12, "Vector3 must be 12 bytes.");
    static_assert(sizeof(Vector4) == 16, "Vector4 must be 16 bytes.");
    static_assert(sizeof(ParticleCS) == 64, "ParticleCS layout must match HLSL.");
    static_assert(offsetof(ParticleCS, isAlive) == 60, "ParticleCS::isAlive offset must match HLSL.");

    struct GPUParticleViewData {
        Matrix4x4 viewProjection;
        Matrix4x4 billboardMatrix;
    };

    struct UpdateParticleInfo {
        float deltaTime;
        uint32_t particleCount;
        float timeScale;
        uint32_t padding;
    };
    static_assert(sizeof(UpdateParticleInfo) == 16, "UpdateParticleInfo must be 16 bytes.");

    // Resource states are tracked explicitly because compute writes and graphics reads share buffers.
    Microsoft::WRL::ComPtr<ID3D12Resource> gpuParticleResource_;
    uint32_t gpuParticleSrvHandle_ = UINT32_MAX;
    uint32_t gpuParticleUavHandle_ = UINT32_MAX;
    D3D12_RESOURCE_STATES gpuParticleResourceState_ = D3D12_RESOURCE_STATE_COMMON;
    Microsoft::WRL::ComPtr<ID3D12Resource> gpuParticleFreeListIndexResource_;
    uint32_t gpuParticleFreeListIndexUavHandle_ = UINT32_MAX;
    D3D12_RESOURCE_STATES gpuParticleFreeListIndexResourceState_ = D3D12_RESOURCE_STATE_COMMON;
    Microsoft::WRL::ComPtr<ID3D12Resource> gpuParticleFreeListResource_;
    uint32_t gpuParticleFreeListUavHandle_ = UINT32_MAX;
    D3D12_RESOURCE_STATES gpuParticleFreeListResourceState_ = D3D12_RESOURCE_STATE_COMMON;

    Microsoft::WRL::ComPtr<ID3D12Resource> gpuParticleViewResource_;
    GPUParticleViewData* gpuParticleViewData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> gpuParticleUpdateInfoResource_;
    UpdateParticleInfo* gpuParticleUpdateInfo_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> gpuParticleEmitterResource_;
    GPUParticleEmitSettings* gpuParticleEmitter_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> gpuParticleInteractionResource_;
    GPUParticleInteractionSettings* gpuParticleInteraction_ = nullptr;
    bool gpuParticleEmitRequested_ = false;
    bool gpuParticlesInitialized_ = false;
    bool gpuParticleInteractionInitialized_ = false;

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
