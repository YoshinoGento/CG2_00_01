#pragma once

#include "application/magnet/system/MagnetChainSystem.h"

#include <cstddef>
#include <cstdint>

class SrvManager;

namespace magnet {

struct MagnetPrototypeViewData {
	bool healthy = false;
	std::size_t bodyCount = 0;
	std::size_t constraintCount = 0;
	std::size_t activeConstraintCount = 0;
	std::size_t activeTestBallCount = 0;
	float playerSpeed = 0.0f;
	float maximumConstraintError = 0.0f;
	bool chainsAttached = true;
	std::size_t goalHitCount = 0;
	float goalWidth = 0.0f;
};

struct MagnetPrototypeUiRequest {
	MagnetChainSystem::EmitterSettings emitterSettings{};
	bool reset = false;
	bool emergencyStop = false;
	bool emitOne = false;
	bool releaseChains = false;
	bool showGrid = true;
	bool showVelocity = true;
	bool cameraFollow = true;
};

// Presentation-only prototype controls. It never mutates gameplay state directly.
class MagnetPrototypeWindow final {
public:
	[[nodiscard]] MagnetPrototypeUiRequest Draw(
		const MagnetPrototypeViewData& viewData,
		SrvManager* srvManager,
		uint32_t finalDisplaySrvIndex,
		float virtualWidth,
		float virtualHeight);

private:
	void DrawMainMenuBar(const MagnetPrototypeViewData& viewData);
	void BuildDefaultLayout(unsigned int dockspaceId);
	void DrawHierarchy(const MagnetPrototypeViewData& viewData);
	void DrawViewport(
		SrvManager* srvManager,
		uint32_t finalDisplaySrvIndex,
		float virtualWidth,
		float virtualHeight);
	void DrawInspector(
		const MagnetPrototypeViewData& viewData,
		MagnetPrototypeUiRequest& request);
	void DrawMonitor(const MagnetPrototypeViewData& viewData);

	MagnetChainSystem::EmitterSettings emitterSettings_{};
	bool showGrid_ = true;
	bool showVelocity_ = true;
	bool cameraFollow_ = true;
	bool rebuildLayoutRequested_ = false;
};

} // namespace magnet
