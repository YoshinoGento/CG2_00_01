#pragma once

class LineDrawer;

namespace magnet {
class MagnetChainSystem;

// Draws bounded procedural electricity around currently attached magnet balls.
class ChargedChainEffectSystem final {
public:
	void Reset() noexcept;
	void Update(float deltaTime) noexcept;
	void Draw(const MagnetChainSystem& chain, LineDrawer& lineDrawer) const;

private:
	float elapsedSeconds_ = 0.0f;
};
} // namespace magnet
