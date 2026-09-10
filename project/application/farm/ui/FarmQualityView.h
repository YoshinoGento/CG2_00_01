#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/system/FarmCropQualitySystem.h"

namespace farmui {
inline Label QualityHintLabel(FarmQualityFocus focus, bool harvested) noexcept {
    switch (focus) {
    case FarmQualityFocus::Maturity: return harvested ? Label::HintMaturityNext : Label::HintMaturity;
    case FarmQualityFocus::Water: return Label::HintWater;
    case FarmQualityFocus::Terrain: return Label::HintTerrain;
    case FarmQualityFocus::Nutrients: return Label::HintNutrients;
    case FarmQualityFocus::Balanced: return Label::HintBalanced;
    default: return Label::HintUnknown;
    }
}
// Read-only presentation: scores and prices always come from the gameplay System.
inline void BuildQualityView(View& view, const FarmCropQualityResult& quality, const FarmQualityAdvice& advice,
    int tileIndex, bool harvested) {
    view.Add(Label::QualityCurrent, {170,130,450,48}, {Action::Quality,0}, true, !harvested);
    view.Add(Label::QualityHarvest, {646,130,450,48}, {Action::Quality,1}, true, harvested);
    if (!quality.IsValid()) {
        view.Add(harvested ? Label::QualityNoHarvest : Label::NoCrop, {176,254,920,44});
        return;
    }
    view.Add(quality.crop == farm::CropType::Carrot ? Label::Carrot : Label::Turnip, {176,192,150,38});
    if (!harvested) view.Metric(Label::Selected, {646,192,450,38}, 160, tileIndex >= 0 ? "#" + std::to_string(tileIndex) : "--");
    view.radar.visible = true;
    const std::array<float,4> values{{quality.maturity, quality.waterBalance, quality.terrainFit, quality.nutrientBalance}};
    constexpr std::array<Label,4> labels{{Label::QualityMaturity, Label::QualityWater, Label::QualityTerrain, Label::QualityNutrient}};
    for (std::size_t i=0; i<values.size(); ++i) {
        const bool known = std::isfinite(values[i]) && (i != 3 || quality.nutrientKnown);
        view.radar.known[i] = known;
        view.radar.values[i] = known ? std::clamp(values[i], 0.0f, 1.0f) : 0.0f;
        view.Metric(labels[i], {600,242 + static_cast<float>(i)*54,496,42}, 280,
            known ? std::to_string(static_cast<int>(std::lround(view.radar.values[i]*100))) + " / 100" : "--");
    }
    view.Add(Label::QualityScale, {600,448,496,38});
    const Label context = advice.partial ? (harvested ? Label::HintPartialRecord : Label::HintPartialEstimate)
        : (harvested ? Label::QualityRecorded : Label::QualityEstimate);
    view.Add(context, {176,536,920,38});
    view.Metric(harvested ? Label::QualityFinalPrice : Label::Score, {176,492,920,38}, 600,
        std::to_string(quality.score) + " / " + std::to_string(quality.salePrice) + "G");
    view.Add(QualityHintLabel(advice.focus, harvested), {176,578,920,38});
}
}
