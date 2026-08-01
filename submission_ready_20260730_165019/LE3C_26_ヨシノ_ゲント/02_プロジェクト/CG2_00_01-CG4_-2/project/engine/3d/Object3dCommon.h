#pragma once

#include "base/DirectXCommon.h"
#include "math/Matrix.h"
#include <array>
#include <d3d12.h>
#include <cstdint>
#include <wrl.h>

class SrvManager;
class LightingSystem;

class Object3dCommon {
public:
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, LightingSystem* lightingSystem);

	void CommonDrawSettings();
	void BeginObjectPass();
	void EndObjectPass();
	[[nodiscard]] bool IsObjectPassActive() const noexcept { return objectPassActive_; }
	void UpdateDirectionalShadow(const Vector3& lightDirection, const Vector3& focusPosition);
	bool BeginShadowPass();
	void EndShadowPass();
	void SetShadowStrength(float strength);

	// 0: None, 1: Front, 2: Back
	ID3D12PipelineState* GetPipelineState(int cullMode, bool isSkinned = false) {
		const int safeCullMode = (cullMode >= 0 && cullMode < 3) ? cullMode : 2;
		return isSkinned ? skinningPipelineStates_[safeCullMode].Get() : pipelineStates_[safeCullMode].Get();
	}

	DirectXCommon* GetDxCommon() const { return dxCommon_; }
	SrvManager* GetSrvManager() const { return srvManager_; }
	LightingSystem* GetLightingSystem() const { return lightingSystem_; }

	ID3D12RootSignature* GetRootSignature(bool isSkinned = false) const { return isSkinned ? skinningRootSignature_.Get() : rootSignature_.Get(); }
	ID3D12RootSignature* GetSkinningComputeRootSignature() const { return skinningComputeRootSignature_.Get(); }
	ID3D12PipelineState* GetSkinningComputePipelineState() const { return skinningComputePipelineState_.Get(); }
	ID3D12RootSignature* GetShadowRootSignature() const { return shadowRootSignature_.Get(); }
	ID3D12PipelineState* GetShadowPipelineState(bool isSkinned, bool isMirrored) const {
		const std::size_t orientation = isMirrored ? 1u : 0u;
		return isSkinned ? shadowSkinningPipelineStates_[orientation].Get() : shadowPipelineStates_[orientation].Get();
	}
	D3D12_GPU_VIRTUAL_ADDRESS GetShadowSceneBufferAddress() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetShadowMapSrvHandle() const;
	const Matrix4x4& GetLightViewProjectionMatrix() const { return lightViewProjectionMatrix_; }
	bool IsShadowReady() const { return shadowReady_; }

private:
	void CreateRootSignature();
	void CreateSkinningRootSignature();
	void CreateSkinningComputeRootSignature();
	void CreateGraphicsPipelineStates();
	void CreateSkinningComputePipelineState();
	void CreateShadowResources();
	void CreateShadowRootSignature();
	void CreateShadowPipelineStates();
	void TransitionShadowMap(D3D12_RESOURCE_STATES stateAfter);

	struct ShadowSceneData {
		Matrix4x4 lightViewProjection;
		Vector2 texelSize;
		float depthBias;
		float normalBias;
		float strength;
		float padding[3];
	};
	static_assert(sizeof(ShadowSceneData) == 96);

	static constexpr uint32_t kShadowMapSize = 2048;
	static constexpr float kShadowHalfExtent = 30.0f;
	static constexpr float kShadowLightDistance = 60.0f;
	static constexpr float kShadowNearClip = 0.1f;
	static constexpr float kShadowFarClip = 120.0f;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	LightingSystem* lightingSystem_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> skinningRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> skinningComputeRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> shadowRootSignature_;
	std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 3> pipelineStates_;
	std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 3> skinningPipelineStates_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> skinningComputePipelineState_;
	std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 2> shadowPipelineStates_;
	std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 2> shadowSkinningPipelineStates_;

	Microsoft::WRL::ComPtr<ID3D12Resource> shadowMapResource_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> shadowDsvHeap_;
	Microsoft::WRL::ComPtr<ID3D12Resource> shadowSceneResource_;
	ShadowSceneData* shadowSceneData_ = nullptr;
	Matrix4x4 lightViewProjectionMatrix_{};
	uint32_t shadowMapSrvIndex_ = 0;
	D3D12_RESOURCE_STATES shadowMapState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	float shadowStrength_ = 0.82f;
	bool shadowReady_ = false;
	bool objectPassActive_ = false;
};
