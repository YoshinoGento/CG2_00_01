#include "SrvManager.h"
#include <cassert>

/**
 * 初期化処理
 */
void SrvManager::Initialize(DirectXCommon* dxCommon) {
	// 依存するクラスが空でないかチェック
	assert(dxCommon);
	dxCommon_ = dxCommon;

	// 1. 画像の棚（ディスクリプタヒープ）を作成します
	// シェーダーから見えるように shaderVisible を true に設定します
	descriptorHeap_ = dxCommon_->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount, true);

	// 2. このPCのGPUにおいて、画像1枚分のデータがどれくらいのサイズかを取得します
	// これを住所計算（ハンドル計算）のオフセットとして使います
	descriptorSize_ = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

/**
 * 空き番号の確保
 */
uint32_t SrvManager::Allocate() {
	// 最大数を超えていないかチェック
	assert(useIndex_ < kMaxSRVCount);

	// 現在の空き番号を確保して、次に備えてカウントを1進める
	uint32_t index = useIndex_;
	useIndex_++;

	return index;
}

/**
 * 描画前処理
 */
void SrvManager::PreDraw() {
	// 今からこの「画像の棚」を使いますよ、という命令をコマンドリストに送る
	ID3D12DescriptorHeap* ppHeaps[] = { descriptorHeap_.Get() };
	dxCommon_->GetCommandList()->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
}

/**
 * テクスチャ情報の作成
 */
void SrvManager::CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels) {
	// 設定情報の作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = Format; // 画像形式
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャとして扱う
	srvDesc.Texture2D.MipLevels = MipLevels;

	// 指定した番号の「設定用の住所」を取得
	D3D12_CPU_DESCRIPTOR_HANDLE handle = GetCPUDescriptorHandle(srvIndex);

	// GPUにSRV（画像の情報）を作成させる
	dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, handle);
}

/**
 * CPU側ハンドルの計算
 */
D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index) const {
	// 先頭の住所を取得
	D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	// 番号 × 1区画のサイズ を足して、目的の場所の住所を出す
	handle.ptr += (static_cast<unsigned long long>(index) * descriptorSize_);
	return handle;
}

/**
 * GPU側ハンドルの計算
 */
D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index) const {
	// 先頭の住所を取得
	D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
	// CPU側と同じように、番号に応じたオフセットを足す
	handle.ptr += (static_cast<unsigned long long>(index) * descriptorSize_);
	return handle;
}