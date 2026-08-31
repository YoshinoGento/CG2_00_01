#include "effect/PostEffectSystem.h"

#include "base/Logger.h"
#include "base/FrameClock.h"
#include "base/SrvManager.h"
#include "effect/PostEffectManager.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

namespace {
constexpr float kDemoDepthOutlineThreshold = 0.001f;
constexpr float kDemoDepthOutlineIntensity = 1.25f;
constexpr float kDemoDepthOutlineThickness = 2.0f;
constexpr float kDemoLuminanceOutlineThreshold = 0.15f;
constexpr float kDemoLuminanceOutlineIntensity = 1.5f;
constexpr float kDemoLuminanceOutlineThickness = 2.0f;
constexpr float kDemoRadialBlurWidth = 0.012f;
constexpr float kDemoDissolveThreshold = 0.45f;
constexpr float kDemoDissolveEdgeWidth = 0.08f;
constexpr float kDemoDissolveEdgeIntensity = 3.5f;
constexpr Vector3 kDemoDissolveEdgeColor{ 1.0f, 0.28f, 0.04f };
constexpr float kDemoRandomNoiseStrength = 0.62f;
constexpr float kDemoRandomNoiseScale = 240.0f;
constexpr float kDemoRandomNoiseTimeSpeed = 3.5f;
constexpr float kDemoGaussianSigma = 1.25f;

float ClampFinite(float value, float minimum, float maximum, float fallback) noexcept {
	return std::clamp(std::isfinite(value) ? value : fallback, minimum, maximum);
}
}

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

void PostEffectSystem::ApplySettings(const Settings& settings) noexcept {
	settings_ = settings;
	const int maximumEffect =
		static_cast<int>(DirectXCommon::FullscreenPostEffectType::GaussianFilter);
	settings_.effect = static_cast<DirectXCommon::FullscreenPostEffectType>(
		std::clamp(static_cast<int>(settings.effect), 0, maximumEffect));
	settings_.grayscaleIntensity =
		ClampFinite(settings.grayscaleIntensity, 0.0f, 1.0f, 1.0f);
	settings_.sepiaIntensity =
		ClampFinite(settings.sepiaIntensity, 0.0f, 1.0f, 1.0f);
	settings_.blurStrength =
		ClampFinite(settings.blurStrength, 0.0f, 16.0f, 4.0f);
	settings_.gaussianSigma =
		ClampFinite(settings.gaussianSigma, 0.1f, 4.0f, 1.0f);
	settings_.bloomThreshold =
		ClampFinite(settings.bloomThreshold, 0.0f, 1.0f, 0.65f);
	settings_.bloomIntensity =
		ClampFinite(settings.bloomIntensity, 0.0f, 8.0f, 1.5f);
	settings_.bloomRadius =
		ClampFinite(settings.bloomRadius, 0.0f, 32.0f, 8.0f);
	settings_.bloomSoftKnee =
		ClampFinite(settings.bloomSoftKnee, 0.001f, 1.0f, 0.2f);
	settings_.radialBlurCenter.x =
		ClampFinite(settings.radialBlurCenter.x, 0.0f, 1.0f, 0.5f);
	settings_.radialBlurCenter.y =
		ClampFinite(settings.radialBlurCenter.y, 0.0f, 1.0f, 0.5f);
	settings_.radialBlurWidth =
		ClampFinite(settings.radialBlurWidth, 0.0f, 0.1f, 0.01f);
	settings_.radialBlurIntensity =
		ClampFinite(settings.radialBlurIntensity, 0.0f, 1.0f, 1.0f);
	settings_.radialBlurSampleCount =
		std::clamp(settings.radialBlurSampleCount, 1, 32);
	settings_.dissolveThreshold =
		ClampFinite(settings.dissolveThreshold, 0.0f, 1.0f, 0.5f);
	settings_.dissolveEdgeWidth =
		ClampFinite(settings.dissolveEdgeWidth, 0.001f, 0.2f, 0.03f);
	settings_.dissolveEdgeIntensity =
		ClampFinite(settings.dissolveEdgeIntensity, 0.0f, 5.0f, 1.0f);
	settings_.dissolveEdgeColor.x =
		ClampFinite(settings.dissolveEdgeColor.x, 0.0f, 1.0f, 1.0f);
	settings_.dissolveEdgeColor.y =
		ClampFinite(settings.dissolveEdgeColor.y, 0.0f, 1.0f, 0.4f);
	settings_.dissolveEdgeColor.z =
		ClampFinite(settings.dissolveEdgeColor.z, 0.0f, 1.0f, 0.3f);
	settings_.outlineThreshold =
		ClampFinite(settings.outlineThreshold, 0.0f, 1.0f, 0.15f);
	settings_.outlineIntensity =
		ClampFinite(settings.outlineIntensity, 0.0f, 8.0f, 1.0f);
	settings_.outlineThickness =
		ClampFinite(settings.outlineThickness, 0.0f, 8.0f, 1.0f);
	settings_.depthOutlineThreshold =
		ClampFinite(settings.depthOutlineThreshold, 0.0f, 0.1f, 0.001f);
	settings_.depthOutlineIntensity =
		ClampFinite(settings.depthOutlineIntensity, 0.0f, 8.0f, 1.0f);
	settings_.depthOutlineThickness =
		ClampFinite(settings.depthOutlineThickness, 1.0f, 8.0f, 1.0f);
	settings_.normalOutlineThreshold =
		ClampFinite(settings.normalOutlineThreshold, 0.0f, 4.0f, 0.25f);
	settings_.normalOutlineIntensity =
		ClampFinite(settings.normalOutlineIntensity, 0.0f, 8.0f, 1.0f);
	settings_.normalOutlineThickness =
		ClampFinite(settings.normalOutlineThickness, 1.0f, 8.0f, 1.0f);
	settings_.vignetteScale =
		ClampFinite(settings.vignetteScale, 0.0f, 64.0f, 16.0f);
	settings_.vignettePower =
		ClampFinite(settings.vignettePower, 0.01f, 8.0f, 0.8f);
	settings_.vignetteIntensity =
		ClampFinite(settings.vignetteIntensity, 0.0f, 1.0f, 1.0f);
	settings_.randomNoiseTime =
		std::isfinite(settings.randomNoiseTime) ? settings.randomNoiseTime : 0.0f;
	settings_.randomNoiseStrength =
		ClampFinite(settings.randomNoiseStrength, 0.0f, 1.0f, 0.2f);
	settings_.randomNoiseScale =
		ClampFinite(settings.randomNoiseScale, 1.0f, 2000.0f, 800.0f);
	settings_.randomNoiseTimeSpeed =
		ClampFinite(settings.randomNoiseTimeSpeed, 0.0f, 10.0f, 1.0f);
	settings_.randomNoiseMode = std::clamp(settings.randomNoiseMode, 0, 1);
	settings_.hsvHue = ClampFinite(settings.hsvHue, -1.0f, 1.0f, 0.0f);
	settings_.hsvSaturation =
		ClampFinite(settings.hsvSaturation, -1.0f, 1.0f, 0.0f);
	settings_.hsvValue = ClampFinite(settings.hsvValue, -1.0f, 1.0f, 0.0f);
	if (!noiseNames_.empty()) {
		settings_.selectedNoiseIndex =
			(std::min)(settings.selectedNoiseIndex, noiseNames_.size() - 1);
	} else {
		settings_.selectedNoiseIndex = 0;
	}
}

void PostEffectSystem::ApplyDemoPreset(DemoPreset preset) {
	// CG5 comparison uses one effect, so ChainMode must not overwrite the selected pass.
	SetChainModeEnabled(false);
	switch (preset) {
	case DemoPreset::Original:
		settings_.effect = DirectXCommon::FullscreenPostEffectType::Copy;
		break;
	case DemoPreset::RequiredGrayscale:
		settings_.effect = DirectXCommon::FullscreenPostEffectType::Grayscale;
		settings_.grayscaleIntensity = 1.0f;
		break;
	case DemoPreset::Vignette:
		settings_.effect = DirectXCommon::FullscreenPostEffectType::Vignette;
		settings_.vignetteScale = 16.0f;
		settings_.vignettePower = 0.8f;
		settings_.vignetteIntensity = 1.0f;
		break;
	case DemoPreset::BoxFilter:
		settings_.effect = DirectXCommon::FullscreenPostEffectType::BoxFilter3x3;
		break;
	case DemoPreset::GaussianFilter:
		settings_.effect = DirectXCommon::FullscreenPostEffectType::GaussianFilter;
		settings_.gaussianSigma = kDemoGaussianSigma;
		break;
	case DemoPreset::LuminanceBasedOutline:
		settings_.effect = DirectXCommon::FullscreenPostEffectType::OutlineLuminance;
		settings_.outlineThreshold = kDemoLuminanceOutlineThreshold;
		settings_.outlineIntensity = kDemoLuminanceOutlineIntensity;
		settings_.outlineThickness = kDemoLuminanceOutlineThickness;
		break;
	case DemoPreset::DepthBasedOutline:
		settings_.effect = DirectXCommon::FullscreenPostEffectType::OutlineDepth;
		SetDepthOutlineParameters({
			.threshold = kDemoDepthOutlineThreshold,
			.intensity = kDemoDepthOutlineIntensity,
			.thickness = kDemoDepthOutlineThickness,
			.linearize = true,
		});
		break;
	case DemoPreset::RadialBlur:
		settings_.effect = DirectXCommon::FullscreenPostEffectType::RadialBlur;
		settings_.radialBlurCenter = { 0.5f, 0.5f };
		settings_.radialBlurWidth = kDemoRadialBlurWidth;
		settings_.radialBlurIntensity = 1.0f;
		settings_.radialBlurSampleCount = 12;
		break;
	case DemoPreset::Dissolve:
		settings_.effect = DirectXCommon::FullscreenPostEffectType::Dissolve;
		settings_.dissolveThreshold = kDemoDissolveThreshold;
		settings_.dissolveEdgeWidth = kDemoDissolveEdgeWidth;
		settings_.dissolveEdgeIntensity = kDemoDissolveEdgeIntensity;
		settings_.dissolveEnableEdge = true;
		settings_.dissolveEdgeColor = kDemoDissolveEdgeColor;
		settings_.selectedNoiseIndex = 0;
		break;
	case DemoPreset::RandomNoise:
		settings_.effect = DirectXCommon::FullscreenPostEffectType::RandomNoise;
		settings_.randomNoiseMode = 1;
		settings_.randomNoiseStrength = kDemoRandomNoiseStrength;
		settings_.randomNoiseScale = kDemoRandomNoiseScale;
		settings_.randomNoiseTimeSpeed = kDemoRandomNoiseTimeSpeed;
		settings_.randomNoiseAnimate = true;
		break;
	default:
		break;
	}
}

void PostEffectSystem::SetGrayscaleIntensity(float intensity) noexcept {
	settings_.grayscaleIntensity = std::clamp(intensity, 0.0f, 1.0f);
}

void PostEffectSystem::SetDissolveThreshold(float threshold) noexcept {
	settings_.dissolveThreshold = ClampFinite(threshold, 0.0f, 1.0f, 0.0f);
}

void PostEffectSystem::SetDepthOutlineParameters(
	const DepthOutlineParameters& parameters) noexcept {
	settings_.depthOutlineThreshold = std::clamp(parameters.threshold, 0.0f, 0.1f);
	settings_.depthOutlineIntensity = std::clamp(parameters.intensity, 0.0f, 8.0f);
	settings_.depthOutlineThickness = std::clamp(parameters.thickness, 1.0f, 8.0f);
	settings_.depthOutlineLinearize = parameters.linearize;
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
	common.gaussianSigma = settings_.gaussianSigma;
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
