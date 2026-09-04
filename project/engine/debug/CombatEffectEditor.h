#pragma once

#ifdef USE_IMGUI

#include "effect/ProceduralCombatEffect.h"

class CombatEffectEditor final {
public:
	void DrawLightning(LightningEffect& effect, const Vector3& origin,
		bool previewRequested = false);
	void DrawSlash(SlashEffect& effect, const Vector3& origin,
		bool previewRequested = false);

private:
	LightningEffectSettings lightning_{};
	SlashEffectSettings slash_{};
};

#endif
