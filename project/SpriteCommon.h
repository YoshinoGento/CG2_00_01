#pragma once
#include "DirectXCommon.h"

class SpriteCommon {
public:

	void Initialize(DirectXCommon * dxCommon);



private:
    
    // ルートシグネチャ作成
    void CreateRootSignature();

    // パイプラインステート作成
    void CreateGraphicsPipelineState();

private:

	DirectXCommon* dxCommon_ ;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;


};

