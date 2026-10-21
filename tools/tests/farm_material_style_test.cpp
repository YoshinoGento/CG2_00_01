#include "farm/render/FarmMaterialStyle.h"
#include <cassert>
#include <iostream>
#include <limits>

int main() {
	using namespace farm;
	unsigned cases = 0;
	for (int surface = 0; surface <= static_cast<int>(FarmMeshSurface::Grass); ++surface) {
		for (int shape = 0; shape <= static_cast<int>(FarmMeshShape::PumpkinVines); ++shape) {
			for (float wet : {-1.0f, 0.0f, 0.1f, 0.5f, 1.0f, 2.0f,
				std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
				FarmMeshPart part;
				part.surface = static_cast<FarmMeshSurface>(surface);
				part.shape = static_cast<FarmMeshShape>(shape);
				part.wetness = wet;
				const auto style = BuildFarmMaterialStyle(part);
				assert(std::isfinite(style.shininess) && style.shininess >= 1.0f);
				assert(style.detail.x >= 0.0f && style.detail.x <= 0.085f);
				assert(style.detail.y >= 0.0f && style.detail.y <= 0.26f);
				assert(style.detail.z >= 0.0f && style.detail.z <= 12.0f);
				assert(style.detail.w >= 0.0f && style.detail.w <= 0.12f);
				if (part.surface == FarmMeshSurface::CanalBank ||
					part.surface == FarmMeshSurface::CanalBed) assert(style.detail.x == 0.0f);
				if (part.surface == FarmMeshSurface::Crop) assert(style.detail.z == 0.0f);
				part.water = true;
				const auto water = BuildFarmMaterialStyle(part);
				assert(water.detail.x == 0.0f && water.detail.y == 0.0f && water.detail.w == 0.0f);
				++cases;
			}
		}
	}
	FarmMeshPart soil; soil.surface = FarmMeshSurface::Soil;
	auto previous = BuildFarmMaterialStyle(soil);
	for (int i = 1; i <= 1000; ++i) {
		soil.wetness = static_cast<float>(i) / 1000.0f;
		const auto style = BuildFarmMaterialStyle(soil);
		assert(style.detail.x >= previous.detail.x);
		assert(style.detail.x - previous.detail.x < 0.0002f);
		assert(style.detail.y <= previous.detail.y && style.detail.w <= previous.detail.w);
		previous = style;
	}
	std::cout << "PASS: " << cases << " material cases, 1000 wetness steps; GPU visuals separate\n";
}
