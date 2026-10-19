#include "farm/render/FarmMeshLayout.h"
#include <iostream>
#include <iomanip>

int main() {
    using namespace farm;
    const auto mesh = [](FarmMeshShape shape) -> const char* {
        switch (shape) {
        case FarmMeshShape::Carrot: return "crop_carrot.obj";
        case FarmMeshShape::CarrotLeaves: return "crop_carrot_leaves.obj";
        case FarmMeshShape::Tomato: return "crop_tomato.obj";
        case FarmMeshShape::TomatoStems: return "crop_tomato_stems.obj";
        case FarmMeshShape::Pumpkin: return "crop_pumpkin.obj";
        case FarmMeshShape::PumpkinVines: return "crop_pumpkin_vines.obj";
        default: return nullptr;
        }
    };
    std::cout << std::setprecision(9) << "[";
    for (int i = 0; i < kPlayableCropCount; ++i) {
        FarmTileVisualData visual{};
        visual.valid = true;
        visual.crop = PlayableCrop(i);
        visual.cropStage = FarmCropGrowthStage::Ready;
        visual.cropScale = 1;
        const auto parts = BuildFarmCropMeshParts(visual, 1);
        if (parts.count != 2) return 1;
        if (i) std::cout << ",";
        std::cout << "{\"cropSlot\":" << ToCropSlot(visual.crop) << ",\"parts\":[";
        for (std::size_t j = 0; j < parts.count; ++j) {
            const auto& p = parts.parts[j];
            if (!mesh(p.shape)) return 2;
            if (j) std::cout << ",";
            std::cout << "{\"mesh\":\"" << mesh(p.shape) << "\",\"position\":["
                << p.position.x << ',' << p.position.y << ',' << p.position.z
                << "],\"scale\":[" << p.scale.x << ',' << p.scale.y << ',' << p.scale.z
                << "],\"color\":[" << p.color.x << ',' << p.color.y << ',' << p.color.z << "]}";
        }
        std::cout << "]}";
    }
    std::cout << "]\n";
}
