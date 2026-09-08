#include "3d/LightingSystem.h"

#include "base/DirectXCommon.h"
#include "base/Logger.h"
#include "math/Matrix.h"

#include <algorithm>
#include <cassert>
#include <cmath>

bool LightingSystem::Initialize(DirectXCommon* dxCommon) {
	if (dxCommon == nullptr) {
		Logger::Log("LightingSystem::Initialize failed. DirectXCommon is null.");
		return false;
	}
	dxCommon_ = dxCommon;
	directionalLightResource_ = dxCommon_->CreateBufferResource(kConstantBufferSize);
	spotLightResource_ = dxCommon_->CreateBufferResource(kConstantBufferSize);
	cameraResource_ = dxCommon_->CreateBufferResource(kConstantBufferSize);
	if (!directionalLightResource_ || !spotLightResource_ || !cameraResource_) {
		Logger::Log("LightingSystem::Initialize failed to create constant buffers.");
		return false;
	}

	D3D12_RANGE readRange{ 0, 0 };
	HRESULT hr = directionalLightResource_->Map(
		0, &readRange, reinterpret_cast<void**>(&mappedDirectionalLight_));
	if (FAILED(hr)) {
		Logger::Log("LightingSystem failed to map the directional-light buffer.");
		return false;
	}
	hr = spotLightResource_->Map(0, &readRange, reinterpret_cast<void**>(&mappedSpotLight_));
	if (FAILED(hr)) {
		Logger::Log("LightingSystem failed to map the spot-light buffer.");
		return false;
	}
	hr = cameraResource_->Map(0, &readRange, reinterpret_cast<void**>(&mappedCamera_));
	if (FAILED(hr)) {
		Logger::Log("LightingSystem failed to map the camera buffer.");
		return false;
	}

	SetDirectionalLight({
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		{ 0.0f, -1.0f, 1.0f },
		1.0f });
	SpotLight defaultSpotLight{};
	defaultSpotLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	defaultSpotLight.position = { 0.0f, 4.0f, 0.0f };
	defaultSpotLight.intensity = 0.0f;
	defaultSpotLight.direction = { 0.0f, -1.0f, 0.0f };
	defaultSpotLight.distance = 10.0f;
	defaultSpotLight.decay = 1.0f;
	defaultSpotLight.cosAngle = std::cos(0.5f);
	defaultSpotLight.cosFalloffStart = std::cos(0.3f);
	SetSpotLight(defaultSpotLight);
	SetCameraPosition({ 0.0f, 0.0f, 0.0f });
	return true;
}

void LightingSystem::SetDirectionalLight(const DirectionalLight& light) {
	directionalLight_ = light;
	directionalLight_.color.x = std::clamp(directionalLight_.color.x, 0.0f, 1.0f);
	directionalLight_.color.y = std::clamp(directionalLight_.color.y, 0.0f, 1.0f);
	directionalLight_.color.z = std::clamp(directionalLight_.color.z, 0.0f, 1.0f);
	directionalLight_.color.w = std::clamp(directionalLight_.color.w, 0.0f, 1.0f);
	directionalLight_.direction = MatrixMath::Normalize(directionalLight_.direction);
	if (MatrixMath::Length(directionalLight_.direction) < 1.0e-4f) {
		directionalLight_.direction = MatrixMath::Normalize(Vector3{ 0.0f, -1.0f, 1.0f });
	}
	directionalLight_.intensity = (std::max)(directionalLight_.intensity, 0.0f);
	if (mappedDirectionalLight_ != nullptr) {
		*mappedDirectionalLight_ = directionalLight_;
	}
}

void LightingSystem::SetSpotLight(const SpotLight& light) {
	spotLight_ = light;
	spotLight_.color.x = std::clamp(spotLight_.color.x, 0.0f, 1.0f);
	spotLight_.color.y = std::clamp(spotLight_.color.y, 0.0f, 1.0f);
	spotLight_.color.z = std::clamp(spotLight_.color.z, 0.0f, 1.0f);
	spotLight_.color.w = std::clamp(spotLight_.color.w, 0.0f, 1.0f);
	spotLight_.intensity = (std::max)(spotLight_.intensity, 0.0f);
	spotLight_.direction = MatrixMath::Normalize(spotLight_.direction);
	if (MatrixMath::Length(spotLight_.direction) < 1.0e-4f) {
		spotLight_.direction = { 0.0f, -1.0f, 0.0f };
	}
	spotLight_.distance = (std::max)(spotLight_.distance, 1.0e-4f);
	spotLight_.decay = (std::max)(spotLight_.decay, 0.0f);
	// FalloffStart must remain strictly inside the outer cone to avoid a zero denominator in HLSL.
	spotLight_.cosAngle = std::clamp(spotLight_.cosAngle, -1.0f, 1.0f - 1.0e-4f);
	spotLight_.cosFalloffStart = std::clamp(
		spotLight_.cosFalloffStart, spotLight_.cosAngle + 1.0e-4f, 1.0f);
	spotLight_.padding = 0.0f;
	if (mappedSpotLight_ != nullptr) {
		*mappedSpotLight_ = spotLight_;
	}
}

void LightingSystem::SetCameraPosition(const Vector3& worldPosition) {
	camera_.worldPosition = worldPosition;
	camera_.padding = 0.0f;
	if (mappedCamera_ != nullptr) {
		*mappedCamera_ = camera_;
	}
}

D3D12_GPU_VIRTUAL_ADDRESS LightingSystem::GetDirectionalLightAddress() const noexcept {
	return directionalLightResource_ ? directionalLightResource_->GetGPUVirtualAddress() : 0;
}

D3D12_GPU_VIRTUAL_ADDRESS LightingSystem::GetSpotLightAddress() const noexcept {
	return spotLightResource_ ? spotLightResource_->GetGPUVirtualAddress() : 0;
}

D3D12_GPU_VIRTUAL_ADDRESS LightingSystem::GetCameraAddress() const noexcept {
	return cameraResource_ ? cameraResource_->GetGPUVirtualAddress() : 0;
}
