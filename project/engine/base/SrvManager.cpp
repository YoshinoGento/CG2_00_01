#include "SrvManager.h"
#include <cassert>
#include "DirectXCommon.h"
#include "Logger.h"

void SrvManager::Initialize(DirectXCommon* dxCommon) {
	assert(dxCommon);
	dxCommon_ = dxCommon;
	descriptorHeap_ = dxCommon_->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount, true);
	descriptorSize_ = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	allocated_.fill(false);
	freeIndices_.clear();
	nextUnusedIndex_ = 0;
	allocatedCount_ = 0;
}

uint32_t SrvManager::Allocate() {
	uint32_t index = kInvalidIndex;
	if (!freeIndices_.empty()) {
		index = freeIndices_.back();
		freeIndices_.pop_back();
	} else if (nextUnusedIndex_ < kMaxSRVCount) {
		index = nextUnusedIndex_++;
	}

	if (index == kInvalidIndex) {
		Logger::Log("SrvManager::Allocate failed. The shader-visible descriptor heap is full.");
		assert(false && "SRV descriptor heap exhausted");
		return kInvalidIndex;
	}
	allocated_[index] = true;
	++allocatedCount_;
	return index;
}

void SrvManager::Release(uint32_t index) {
	if (!IsAllocated(index)) {
		Logger::Log("SrvManager::Release rejected an index that is not allocated.");
		assert(false && "Invalid or duplicate SRV descriptor release");
		return;
	}
	allocated_[index] = false;
	--allocatedCount_;
	freeIndices_.push_back(index);
}

void SrvManager::PreDraw() {
	ID3D12DescriptorHeap* ppHeaps[] = { descriptorHeap_.Get() };
	dxCommon_->GetCommandList()->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
}

void SrvManager::CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels) {
	if (!IsAllocated(srvIndex) || pResource == nullptr || MipLevels == 0) {
		Logger::Log("SrvManager::CreateSRVforTexture2D rejected invalid input.");
		assert(false && "Invalid Texture2D SRV input");
		return;
	}
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = MipLevels;
	dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));
}

void SrvManager::CreateSRVforTextureCube(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels) {
	if (!IsAllocated(srvIndex) || pResource == nullptr || mipLevels == 0) {
		Logger::Log("SrvManager::CreateSRVforTextureCube rejected invalid input.");
		assert(false && "Invalid TextureCube SRV input");
		return;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = mipLevels;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));
}

void SrvManager::CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride) {
	if (!IsAllocated(srvIndex) || pResource == nullptr || numElements == 0 || structureByteStride == 0) {
		Logger::Log("SrvManager::CreateSRVforStructuredBuffer rejected invalid input.");
		assert(false && "Invalid structured-buffer SRV input");
		return;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = numElements;
	srvDesc.Buffer.StructureByteStride = structureByteStride;
	dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));
}

void SrvManager::CreateUAVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride) {
	if (!IsAllocated(srvIndex) || pResource == nullptr || numElements == 0 || structureByteStride == 0) {
		Logger::Log("SrvManager::CreateUAVforStructuredBuffer rejected invalid input.");
		assert(false && "Invalid structured-buffer UAV input");
		return;
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = numElements;
	uavDesc.Buffer.StructureByteStride = structureByteStride;
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	dxCommon_->GetDevice()->CreateUnorderedAccessView(pResource, nullptr, &uavDesc, GetCPUDescriptorHandle(srvIndex));
}

D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index) const {
	if (descriptorHeap_ == nullptr || !IsAllocated(index)) {
		Logger::Log("SrvManager::GetCPUDescriptorHandle rejected an unallocated index.");
		assert(false && "Invalid CPU descriptor index");
		return {};
	}
	D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += (static_cast<unsigned long long>(index) * descriptorSize_);
	return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index) const {
	if (descriptorHeap_ == nullptr || !IsAllocated(index)) {
		Logger::Log("SrvManager::GetGPUDescriptorHandle rejected an unallocated index.");
		assert(false && "Invalid GPU descriptor index");
		return {};
	}
	D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += (static_cast<unsigned long long>(index) * descriptorSize_);
	return handle;
}
