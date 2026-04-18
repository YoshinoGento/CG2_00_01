#pragma once
#include "DirectXCommon.h"
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>

/**
 * SrvManagerクラス
 * テクスチャ（SRV）の管理を行うクラスです。
 */
class SrvManager {
public:
	static const uint32_t kMaxSRVCount = 512;

	void Initialize(DirectXCommon* dxCommon);
	void PreDraw();
	uint32_t Allocate();
	void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);

	ID3D12DescriptorHeap* GetSrvDescriptorHeap() const { return descriptorHeap_.Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index) const;

private:
	DirectXCommon* dxCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
	uint32_t descriptorSize_ = 0;
	uint32_t useIndex_ = 0;
};