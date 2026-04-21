#pragma once
#include "base/DirectXCommon.h"
#include <wrl.h>
#include <d3d12.h>
#include <array>

class SrvManager;

/**
 * Object3dCommonクラス
 * カリング設定に応じた複数のパイプラインを保持するように拡張
 */
class Object3dCommon {
public:
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	// 共通描画設定
	void CommonDrawSettings();

	// ★追加：カリングモードに応じたPSOを取得
	// 0: None, 1: Front, 2: Back
	ID3D12PipelineState* GetPipelineState(int cullMode) { return pipelineStates_[cullMode].Get(); }

	DirectXCommon* GetDxCommon() const { return dxCommon_; }
	SrvManager* GetSrvManager() const { return srvManager_; }

private:
	void CreateRootSignature();
	void CreateGraphicsPipelineStates(); // ★複数作成するように変更

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	// ★3種類（なし、前、後ろ）のPipelineStateを保持
	std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 3> pipelineStates_;
};