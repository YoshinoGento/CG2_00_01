#pragma once
#include <d3d12.h>
class ResourceObject {
public:
	ResourceObject(ID3D12Resource* resource) 
		:resource_(resource)
	{}
	//デストラクタはオブジェクトの寿命がつきたときによばれる
	~ResourceObject() {
		//ここでReleaseを呼べばいい
		resource_->Release();
	}
	ID3D12Resource* Get(){return resource_;}
private:
	ID3D12Resource* resource_;
};

