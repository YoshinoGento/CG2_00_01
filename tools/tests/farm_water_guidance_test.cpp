#include "farm/system/FarmGrowthSystem.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

int main() try {
    using farm::FarmWaterStatus;
    using farm::FarmTileState;
    FarmGrowthSystem growth;
    growth.Initialize();
    Require(!growth.AnalyzeWater(nullptr, {}, FarmWaterStatus::None).visible, "null selection");
    farm::FarmTile tile;
    for (auto feature : {farm::FarmTileFeature::Canal, farm::FarmTileFeature::WaterSource}) {
        tile.feature = feature;
        Require(!growth.AnalyzeWater(&tile, {}, FarmWaterStatus::None).visible, "non soil");
    }
    tile.feature = farm::FarmTileFeature::None;
    Require(growth.AnalyzeWater(&tile, {}, FarmWaterStatus::None).advice == FarmWaterAdvice::Till, "empty");
    for (auto state : {FarmTileState::Tilled, FarmTileState::Watered}) {
        tile.state = state; tile.moisture = 1;
        Require(growth.AnalyzeWater(&tile, {}, FarmWaterStatus::Available).advice == FarmWaterAdvice::Plant, "no drying promise without crop");
    }
    tile.state = FarmTileState::Planted;
    int cases = 0;
    for (auto crop : {farm::CropType::TestCrop, farm::CropType::Carrot})
    for (float speed : {0.0f, 1.0f, 2.0f, 4.0f})
    for (bool enabled : {false, true})
    for (auto supply : {FarmWaterStatus::None, FarmWaterStatus::Available, FarmWaterStatus::Retained,
        FarmWaterStatus::Waiting, FarmWaterStatus::Dry}) {
        tile.crop = crop; tile.irrigationEnabled = enabled; tile.growth = 0.25f;
        const bool available = enabled && (supply == FarmWaterStatus::Available || supply == FarmWaterStatus::Retained);
        const float strength = available ? 0.9f : 0.0f;
        const auto profile = growth.Evaluate(tile, crop, speed, strength);
        for (float moisture : {0.0f, profile.goodMoistureMinimum * 0.9f,
            profile.goodMoistureMinimum, profile.goodMoistureMaximum, 1.0f}) {
            tile.moisture = moisture;
            const auto forecast = growth.Evaluate(tile, crop, speed, strength);
            const auto guidance = growth.AnalyzeWater(&tile, forecast, supply);
            const auto expected = moisture < profile.goodMoistureMinimum
                ? (available ? FarmWaterAdvice::CheckSupply : FarmWaterAdvice::Water)
                : moisture > profile.goodMoistureMaximum
                ? (available ? FarmWaterAdvice::CloseIntake : FarmWaterAdvice::AvoidWater)
                : FarmWaterAdvice::Monitor;
            Require(guidance.visible && guidance.intakeClosed == !enabled, "intake projection");
            Require(guidance.supply == supply && guidance.advice == expected, "crop/speed/moisture advice");
            Require(tile.moisture == moisture && tile.growth == 0.25f && tile.irrigationEnabled == enabled, "read only");
            ++cases;
        }
        tile.growth = 1;
        Require(growth.AnalyzeWater(&tile, growth.Evaluate(tile, crop, speed, strength), supply).advice == FarmWaterAdvice::Harvest, "harvest takes priority");
    }
    tile.growth = 0.5f;
    for (float invalid : {-1.0f, 2.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
        tile.moisture = invalid;
        Require(growth.AnalyzeWater(&tile, {}, FarmWaterStatus::None).advice == FarmWaterAdvice::Unknown, "invalid moisture");
        tile.moisture = 0.5f; tile.growth = invalid;
        Require(growth.AnalyzeWater(&tile, {}, FarmWaterStatus::None).advice == FarmWaterAdvice::Unknown, "invalid growth");
        tile.growth = 0.5f;
    }
    auto forecast = growth.Evaluate(tile);
    forecast.profileCrop = farm::CropType::TestCrop;
    Require(growth.AnalyzeWater(&tile, forecast, FarmWaterStatus::None).advice == FarmWaterAdvice::Unknown, "mismatched forecast");
    tile.crop = farm::CropType::None;
    Require(growth.AnalyzeWater(&tile, forecast, FarmWaterStatus::None).advice == FarmWaterAdvice::Unknown, "invalid planted crop");
    std::cout << "PASS water guidance: " << cases << " crop/speed/intake/supply/moisture cases and guards\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
