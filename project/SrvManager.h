#pragma once
#include "DirectXCommon.h"

class SrvManager {
public:
    // 初期化
    void Initialize(DirectXCommon* dxCommon);

    // 空き番号を確保する
    uint32_t Allocate();

    // デスクリプタハンドル取得
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

    // テクスチャ用SRV生成関数
    void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);

    // 描画前処理 (Heapをコマンドリストにセット)
    void PreDraw();

    // ゲッター
    ID3D12DescriptorHeap* GetSrvDescriptorHeap() const { return descriptorHeap_.Get(); }

    // 最大テクスチャ数
    static const uint32_t kMaxSRVCount = 512;

private:
    DirectXCommon* dxCommon_ = nullptr;

    // SRV用デスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
    // 1つあたりのサイズ
    uint32_t descriptorSize_;
    // 次に使うインデックス
    uint32_t useIndex_ = 0;
};