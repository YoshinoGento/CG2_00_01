#pragma once

#include <cstddef>
#include <cstdint>

class Input;
class PostEffectSystem;
class WinApp;

// Release-only evaluator controls. Runtime PostEffect ownership stays in PostEffectSystem.
class PostEffectSubmissionDemo final {
public:
	void Initialize(PostEffectSystem& postEffectSystem, WinApp& winApp);
	void Update(
		float deltaTime,
		Input& input,
		PostEffectSystem& postEffectSystem,
		WinApp& winApp);

	[[nodiscard]] const char* GetCurrentEffectName() const noexcept;
	[[nodiscard]] bool IsDissolveActive() const noexcept;
	[[nodiscard]] uint32_t GetDissolvePercent() const noexcept;

private:
	void ApplyCurrentPreset(PostEffectSystem& postEffectSystem, WinApp& winApp);
	void UpdateDissolve(float deltaTime, PostEffectSystem& postEffectSystem, WinApp& winApp);
	void UpdateWindowTitle(WinApp& winApp) const;

	std::size_t presetIndex_ = 0;
	float dissolveElapsedSeconds_ = 0.0f;
	uint32_t dissolvePercent_ = 0;
};
