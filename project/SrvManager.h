#pragma once
#include "DirectXCommon.h"
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>

/**
 * SrvManagerクラス
 * テクスチャなどの「画像データ」をGPUがどこから読み込めばいいかを管理する窓口です。
 * 専門用語で SRV (Shader Resource View) の管理を行います。
 */
class SrvManager {
public:
	// 最大で管理できる画像の数（512枚まで）
	static const uint32_t kMaxSRVCount = 512;

	/// <summary>
	/// 初期化
	/// </summary>
	/// <param name="dxCommon">DirectX基盤のポインタ</param>
	void Initialize(DirectXCommon* dxCommon);

	/// <summary>
	/// 描画前の準備（コマンドリストに「画像の棚」をセットする）
	/// </summary>
	void PreDraw();

	/// <summary>
	/// 新しい画像用の場所（空き番号）を確保して、その番号を返す
	/// </summary>
	/// <returns>確保した場所の番号（インデックス）</returns>
	uint32_t Allocate();

	/// <summary>
	/// 指定した番号の場所に、テクスチャ(2D画像)の情報を書き込む
	/// </summary>
	/// <param name="srvIndex">Allocateで確保した番号</param>
	/// <param name="pResource">画像データ本体のリソース</param>
	/// <param name="Format">画像の形式（DXGI_FORMAT）</param>
	/// <param name="MipLevels">画像の詳細度（ミップマップ数）</param>
	void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);

	// --- ゲッター群 ---

	// 画像の棚（ディスクリプタヒープ）本体を取得
	ID3D12DescriptorHeap* GetSrvDescriptorHeap() const { return descriptorHeap_.Get(); }

	// 指定した番号の「CPU側」のハンドル（設定するときに使う住所）を取得
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index) const;

	// 指定した番号の「GPU側」のハンドル（描画命令を出すときに使う住所）を取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index) const;

private:
	DirectXCommon* dxCommon_ = nullptr;

	// 画像の場所をまとめた「棚（ヒープ）」
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;

	// 棚の1区画あたりのサイズ（GPUごとに異なるため実行時に取得します）
	// ★警告C26495の修正：宣言時に 0 で初期化します。
	uint32_t descriptorSize_ = 0;

	// 次に使う空き場所の番号（0から順番に埋めていきます）
	uint32_t useIndex_ = 0;
};