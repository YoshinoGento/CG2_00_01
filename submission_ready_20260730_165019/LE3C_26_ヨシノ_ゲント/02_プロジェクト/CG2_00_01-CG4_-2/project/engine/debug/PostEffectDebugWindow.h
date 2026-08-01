#pragma once

#include "effect/PostEffectSystem.h"

#include <cstddef>
#include <optional>

struct PostEffectChainPassChange {
	std::size_t index = 0;
	bool enabled = false;
};

struct PostEffectDebugActions {
	std::optional<PostEffectSystem::Settings> settings;
	std::optional<bool> chainModeEnabled;
	std::optional<PostEffectChainPassChange> chainPassChange;
	bool resetSettings = false;
};

// Displays PostEffect state and emits requests; PostEffectSystem owns mutation.
class PostEffectDebugWindow final {
public:
	[[nodiscard]] PostEffectDebugActions Draw(const PostEffectSystem& system);

private:
	int parameterTarget_ = 1;
};
