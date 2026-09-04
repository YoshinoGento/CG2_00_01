#pragma once

#include "2d/TextureManager.h"
#include "base/DirectXCommon.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class PostEffectManager;
class SrvManager;

class PostEffectSystem final {
public:
	struct Settings {
		DirectXCommon::FullscreenPostEffectType effect = DirectXCommon::FullscreenPostEffectType::Copy;
		float grayscaleIntensity = 1.0f;
		float sepiaIntensity = 1.0f;
		float blurStrength = 4.0f;
		float bloomThreshold = 0.65f;
		float bloomIntensity = 1.5f;
		float bloomRadius = 8.0f;
		float bloomSoftKnee = 0.2f;
		Vector2 radialBlurCenter = { 0.5f, 0.5f };
		float radialBlurWidth = 0.01f;
		float radialBlurIntensity = 1.0f;
		int radialBlurSampleCount = 10;
		float dissolveThreshold = 0.5f;
		float dissolveEdgeWidth = 0.03f;
		float dissolveEdgeIntensity = 1.0f;
		bool dissolveEnableEdge = true;
		Vector3 dissolveEdgeColor = { 1.0f, 0.4f, 0.3f };
		float outlineThreshold = 0.15f;
		float outlineIntensity = 1.0f;
		float outlineThickness = 1.0f;
		float depthOutlineThreshold = 0.001f;
		float depthOutlineIntensity = 1.0f;
		float depthOutlineThickness = 1.0f;
		bool depthOutlineLinearize = true;
		float normalOutlineThreshold = 0.25f;
		float normalOutlineIntensity = 1.0f;
		float normalOutlineThickness = 1.0f;
		float vignetteScale = 16.0f;
		float vignettePower = 0.8f;
		float vignetteIntensity = 1.0f;
		float randomNoiseTime = 0.0f;
		float randomNoiseStrength = 0.2f;
		float randomNoiseScale = 800.0f;
		float randomNoiseTimeSpeed = 1.0f;
		int randomNoiseMode = 1;
		bool randomNoiseAnimate = true;
		float hsvHue = 0.0f;
		float hsvSaturation = 0.0f;
		float hsvValue = 0.0f;
		std::size_t selectedNoiseIndex = 0;
	};

	PostEffectSystem();
	~PostEffectSystem();
	PostEffectSystem(const PostEffectSystem&) = delete;
	PostEffectSystem& operator=(const PostEffectSystem&) = delete;

	bool Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Finalize();
	void Update(float deltaTime);
	void Execute(float nearClip, float farClip);
	void ResetSettings() { settings_ = Settings{}; }

	[[nodiscard]] Settings& GetSettings() noexcept { return settings_; }
	[[nodiscard]] const Settings& GetSettings() const noexcept { return settings_; }
	[[nodiscard]] uint32_t GetFinalDisplaySrvIndex() const noexcept { return finalDisplaySrvIndex_; }
	[[nodiscard]] const std::vector<std::string>& GetNoiseNames() const noexcept { return noiseNames_; }

	void SetChainModeEnabled(bool enabled);
	[[nodiscard]] bool IsChainModeEnabled() const;
	[[nodiscard]] std::size_t GetChainPassCount() const;
	[[nodiscard]] const char* GetChainPassName(std::size_t index) const;
	[[nodiscard]] bool IsChainPassEnabled(std::size_t index) const;
	void SetChainPassEnabled(std::size_t index, bool enabled);
	[[nodiscard]] std::size_t GetEnabledChainPassCount() const;

private:
	bool AllocateResourceDescriptors();
	void ReleaseResourceDescriptors();

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	std::unique_ptr<PostEffectManager> manager_;
	Settings settings_{};
	std::vector<Texture2DHandle> noiseTextureHandles_;
	std::vector<std::string> noiseNames_;
	uint32_t sceneSrvIndex_ = UINT32_MAX;
	uint32_t postEffectResultSrvIndex_ = UINT32_MAX;
	uint32_t finalDisplaySrvIndex_ = UINT32_MAX;
	uint32_t depthSrvIndex_ = UINT32_MAX;
	uint32_t normalSrvIndex_ = UINT32_MAX;
	bool initialized_ = false;
};
