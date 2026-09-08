#pragma once
#include "DirectXCommon.h"
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <limits>
#include <array>
#include <vector>

/**
 * SrvManagerクラス
 * テクスチャ（SRV）の管理を行うクラスです。
 */
class SrvManager {
public:
	static constexpr uint32_t kMaxSRVCount = 512;
	static constexpr uint32_t kInvalidIndex = (std::numeric_limits<uint32_t>::max)();

	void Initialize(DirectXCommon* dxCommon);
	void PreDraw();
	uint32_t Allocate();
	void Release(uint32_t index);
	void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);
	void CreateSRVforTextureCube(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels);
	void CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);
	void CreateUAVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

	ID3D12DescriptorHeap* GetSrvDescriptorHeap() const { return descriptorHeap_.Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index) const;
	[[nodiscard]] bool IsAllocated(uint32_t index) const noexcept {
		return index < kMaxSRVCount && allocated_[index];
	}
	[[nodiscard]] uint32_t GetAllocatedCount() const noexcept { return allocatedCount_; }

private:
	DirectXCommon* dxCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
	uint32_t descriptorSize_ = 0;
	std::array<bool, kMaxSRVCount> allocated_{};
	std::vector<uint32_t> freeIndices_;
	uint32_t nextUnusedIndex_ = 0;
	uint32_t allocatedCount_ = 0;
};
