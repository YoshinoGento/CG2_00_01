#pragma once

#include "math/Struct.h"

#include <d3d12.h>
#include <wrl.h>
#include <cstddef>

class DirectXCommon;

class LightingSystem final {
public:
	struct DirectionalLight {
		Vector4 color;
		Vector3 direction;
		float intensity;
	};
	static_assert(sizeof(DirectionalLight) == 32);

	struct SpotLight {
		Vector4 color;
		Vector3 position;
		float intensity;
		Vector3 direction;
		float distance;
		float decay;
		float cosAngle;
		float cosFalloffStart;
		float padding;
	};
	static_assert(sizeof(SpotLight) == 64);

	struct CameraForGpu {
		Vector3 worldPosition;
		float padding;
	};
	static_assert(sizeof(CameraForGpu) == 16);

	bool Initialize(DirectXCommon* dxCommon);
	void SetDirectionalLight(const DirectionalLight& light);
	void SetSpotLight(const SpotLight& light);
	void SetCameraPosition(const Vector3& worldPosition);

	[[nodiscard]] const DirectionalLight& GetDirectionalLight() const noexcept { return directionalLight_; }
	[[nodiscard]] const SpotLight& GetSpotLight() const noexcept { return spotLight_; }
	[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetDirectionalLightAddress() const noexcept;
	[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetSpotLightAddress() const noexcept;
	[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetCameraAddress() const noexcept;

private:
	static constexpr std::size_t kConstantBufferSize = 256;

	DirectXCommon* dxCommon_ = nullptr;
	DirectionalLight directionalLight_{};
	SpotLight spotLight_{};
	CameraForGpu camera_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
	DirectionalLight* mappedDirectionalLight_ = nullptr;
	SpotLight* mappedSpotLight_ = nullptr;
	CameraForGpu* mappedCamera_ = nullptr;
};
