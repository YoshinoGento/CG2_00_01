#pragma once

#include "base/DirectXCommon.h"
#include <array>
#include <d3d12.h>
#include <wrl.h>

class SrvManager;

class Object3dCommon {
public:
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	void CommonDrawSettings();

	// 0: None, 1: Front, 2: Back
	ID3D12PipelineState* GetPipelineState(int cullMode, bool isSkinned = false) { return isSkinned ? skinningPipelineStates_[cullMode].Get() : pipelineStates_[cullMode].Get(); }

	DirectXCommon* GetDxCommon() const { return dxCommon_; }
	SrvManager* GetSrvManager() const { return srvManager_; }

	ID3D12RootSignature* GetRootSignature(bool isSkinned = false) const { return isSkinned ? skinningRootSignature_.Get() : rootSignature_.Get(); }
	ID3D12RootSignature* GetSkinningComputeRootSignature() const { return skinningComputeRootSignature_.Get(); }
	ID3D12PipelineState* GetSkinningComputePipelineState() const { return skinningComputePipelineState_.Get(); }

private:
	void CreateRootSignature();
	void CreateSkinningRootSignature();
	void CreateSkinningComputeRootSignature();
	void CreateGraphicsPipelineStates();
	void CreateSkinningComputePipelineState();

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> skinningRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> skinningComputeRootSignature_;
	std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 3> pipelineStates_;
	std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 3> skinningPipelineStates_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> skinningComputePipelineState_;
};
