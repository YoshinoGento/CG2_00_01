#include "effect/PostEffectSystem.h"

#include "base/Logger.h"
#include "base/FrameClock.h"
#include "base/SrvManager.h"
#include "effect/PostEffectManager.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

PostEffectSystem::PostEffectSystem() = default;

PostEffectSystem::~PostEffectSystem() {
	assert(!initialized_ && "PostEffectSystem::Finalize must run before destruction.");
}

bool PostEffectSystem::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	if (initialized_ || dxCommon == nullptr || srvManager == nullptr) {
		Logger::Log("PostEffectSystem::Initialize rejected invalid state or dependency.");
		return false;
	}
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	if (!AllocateResourceDescriptors()) {
		ReleaseResourceDescriptors();
		dxCommon_ = nullptr;
		srvManager_ = nullptr;
		return false;
	}

	noiseNames_ = { "noise0.png", "noise1.png" };
	noiseTextureHandles_.reserve(noiseNames_.size());
	for (const std::string& noiseName : noiseNames_) {
		noiseTextureHandles_.push_back(TextureManager::GetInstance()->LoadTexture2D(
			"Resources/" + noiseName, TextureManager::Lifetime::Global));
	}

	manager_ = std::make_unique<PostEffectManager>();
	manager_->Initialize(
		dxCommon_, srvManager_, sceneSrvIndex_, postEffectResultSrvIndex_, finalDisplaySrvIndex_,
		depthSrvIndex_, normalSrvIndex_);
	initialized_ = true;
	return true;
}

void PostEffectSystem::Finalize() {
	if (!initialized_) {
		return;
	}
	if (manager_) {
		manager_->Finalize();
		manager_.reset();
	}
	ReleaseResourceDescriptors();
	noiseTextureHandles_.clear();
	noiseNames_.clear();
	dxCommon_ = nullptr;
	srvManager_ = nullptr;
	initialized_ = false;
}

bool PostEffectSystem::AllocateResourceDescriptors() {
	assert(dxCommon_ != nullptr && srvManager_ != nullptr);
	const std::array<ID3D12Resource*, 5> resources = {
		dxCommon_->GetRenderTextureResource(),
		dxCommon_->GetPostEffectResultResource(),
		dxCommon_->GetFinalDisplayTextureResource(),
		dxCommon_->GetDepthBufferResource(),
		dxCommon_->GetNormalTextureResource(),
	};
	for (ID3D12Resource* resource : resources) {
		if (resource == nullptr) {
			Logger::Log("PostEffectSystem found a null render resource.");
			return false;
		}
	}

	uint32_t* indices[] = {
		&sceneSrvIndex_, &postEffectResultSrvIndex_, &finalDisplaySrvIndex_, &depthSrvIndex_, &normalSrvIndex_
	};
	for (uint32_t* index : indices) {
		*index = srvManager_->Allocate();
		if (*index == SrvManager::kInvalidIndex) {
			return false;
		}
	}

	srvManager_->CreateSRVforTexture2D(sceneSrvIndex_, resources[0], DXGI_FORMAT_R8G8B8A8_UNORM, 1);
	srvManager_->CreateSRVforTexture2D(postEffectResultSrvIndex_, resources[1], DXGI_FORMAT_R8G8B8A8_UNORM, 1);
	srvManager_->CreateSRVforTexture2D(finalDisplaySrvIndex_, resources[2], DXGI_FORMAT_R8G8B8A8_UNORM, 1);
	srvManager_->CreateSRVforTexture2D(depthSrvIndex_, resources[3], DXGI_FORMAT_R24_UNORM_X8_TYPELESS, 1);
	srvManager_->CreateSRVforTexture2D(normalSrvIndex_, resources[4], DXGI_FORMAT_R8G8B8A8_UNORM, 1);
	return true;
}

void PostEffectSystem::ReleaseResourceDescriptors() {
	if (srvManager_ == nullptr) {
		return;
	}
	uint32_t* indices[] = {
		&sceneSrvIndex_, &postEffectResultSrvIndex_, &finalDisplaySrvIndex_, &depthSrvIndex_, &normalSrvIndex_
	};
	for (uint32_t* index : indices) {
		if (*index != SrvManager::kInvalidIndex && srvManager_->IsAllocated(*index)) {
			srvManager_->Release(*index);
		}
		*index = SrvManager::kInvalidIndex;
	}
}

void PostEffectSystem::Update(float deltaTime) {
	if (!initialized_) {
		return;
	}
	if (!std::isfinite(deltaTime)) {
		deltaTime = 0.0f;
	}
	deltaTime = std::clamp(deltaTime, 0.0f, FrameClock::kMaximumFrameDeltaSeconds);
	if (settings_.randomNoiseAnimate) {
		settings_.randomNoiseTime += deltaTime * std::clamp(settings_.randomNoiseTimeSpeed, 0.0f, 10.0f);
	}
}

void PostEffectSystem::Execute(float nearClip, float farClip) {
	if (!initialized_ || manager_ == nullptr) {
		Logger::Log("PostEffectSystem::Execute called before initialization.");
		return;
	}

	DirectXCommon::FullscreenPostEffectParameter common{};
	common.grayscaleIntensity = settings_.grayscaleIntensity;
	common.sepiaIntensity = settings_.sepiaIntensity;
	common.blurStrength = settings_.blurStrength;
	common.bloomThreshold = settings_.bloomThreshold;
	common.bloomIntensity = settings_.bloomIntensity;
	common.bloomRadius = settings_.bloomRadius;
	common.bloomSoftKnee = settings_.bloomSoftKnee;
	common.outlineThreshold = settings_.outlineThreshold;
	common.outlineIntensity = settings_.outlineIntensity;
	common.outlineThickness = settings_.outlineThickness;
	common.depthOutlineThreshold = settings_.depthOutlineThreshold;
	common.depthOutlineIntensity = settings_.depthOutlineIntensity;
	common.depthOutlineThickness = settings_.depthOutlineThickness;
	common.depthOutlineLinearize = settings_.depthOutlineLinearize ? 1.0f : 0.0f;
	common.depthOutlineNearClip = (std::max)(nearClip, 1.0e-4f);
	common.depthOutlineFarClip = (std::max)(farClip, common.depthOutlineNearClip + 1.0e-4f);
	common.normalOutlineThreshold = settings_.normalOutlineThreshold;
	common.normalOutlineIntensity = settings_.normalOutlineIntensity;
	common.normalOutlineThickness = settings_.normalOutlineThickness;
	dxCommon_->SetFullscreenPostEffectParameter(common);

	DirectXCommon::VignetteParamForGPU vignette{};
	vignette.scale = settings_.vignetteScale;
	vignette.power = settings_.vignettePower;
	vignette.intensity = settings_.vignetteIntensity;
	dxCommon_->SetVignetteParameter(vignette);

	DirectXCommon::RadialBlurParamForGPU radialBlur{};
	radialBlur.center = settings_.radialBlurCenter;
	radialBlur.blurWidth = settings_.radialBlurWidth;
	radialBlur.intensity = settings_.radialBlurIntensity;
	radialBlur.sampleCount = settings_.radialBlurSampleCount;
	dxCommon_->SetRadialBlurParameter(radialBlur);

	DirectXCommon::DissolveParamForGPU dissolve{};
	dissolve.threshold = settings_.dissolveThreshold;
	dissolve.edgeWidth = settings_.dissolveEdgeWidth;
	dissolve.edgeIntensity = settings_.dissolveEdgeIntensity;
	dissolve.enableEdge = settings_.dissolveEnableEdge ? 1.0f : 0.0f;
	dissolve.edgeColor = settings_.dissolveEdgeColor;
	dxCommon_->SetDissolveParameter(dissolve);

	DirectXCommon::RandomNoiseParamForGPU randomNoise{};
	randomNoise.time = settings_.randomNoiseTime;
	randomNoise.strength = settings_.randomNoiseStrength;
	randomNoise.scale = settings_.randomNoiseScale;
	randomNoise.mode = static_cast<float>(settings_.randomNoiseMode);
	randomNoise.animate = settings_.randomNoiseAnimate ? 1.0f : 0.0f;
	dxCommon_->SetRandomNoiseParameter(randomNoise);

	DirectXCommon::HSVFilterParamForGPU hsv{};
	hsv.hue = settings_.hsvHue;
	hsv.saturation = settings_.hsvSaturation;
	hsv.value = settings_.hsvValue;
	dxCommon_->SetHSVFilterParameter(hsv);

	D3D12_GPU_DESCRIPTOR_HANDLE auxiliarySrv{};
	const bool needsNoise = IsChainModeEnabled() ||
		settings_.effect == DirectXCommon::FullscreenPostEffectType::Dissolve;
	if (needsNoise && !noiseTextureHandles_.empty()) {
		settings_.selectedNoiseIndex = (std::min)(settings_.selectedNoiseIndex, noiseTextureHandles_.size() - 1);
		auxiliarySrv = TextureManager::GetInstance()->GetGpuHandle(
			noiseTextureHandles_[settings_.selectedNoiseIndex]);
	}
	manager_->Execute(settings_.effect, auxiliarySrv);
}

void PostEffectSystem::SetChainModeEnabled(bool enabled) {
	if (manager_) manager_->SetChainModeEnabled(enabled);
}

bool PostEffectSystem::IsChainModeEnabled() const {
	return manager_ && manager_->IsChainModeEnabled();
}

std::size_t PostEffectSystem::GetChainPassCount() const {
	return manager_ ? manager_->GetChainPassCount() : 0;
}

const char* PostEffectSystem::GetChainPassName(std::size_t index) const {
	return manager_ ? manager_->GetChainPassName(index) : "";
}

bool PostEffectSystem::IsChainPassEnabled(std::size_t index) const {
	return manager_ && manager_->IsChainPassEnabled(index);
}

void PostEffectSystem::SetChainPassEnabled(std::size_t index, bool enabled) {
	if (manager_) manager_->SetChainPassEnabled(index, enabled);
}

std::size_t PostEffectSystem::GetEnabledChainPassCount() const {
	return manager_ ? manager_->GetEnabledChainPassCount() : 0;
}
