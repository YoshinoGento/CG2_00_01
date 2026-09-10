#include "farm/system/FarmPlayFlow.h"
#include "farm/system/FarmProgressionSystem.h"
#include "farm/system/FarmEconomySystem.h"
#include "farm/system/FarmGrowthSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmSoilSystem.h"
#include <limits>
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
	{
		assert(FarmCropQualitySystem::Analyze({}).focus == FarmQualityFocus::None);
		FarmCropQualityResult full;
		full.crop = farm::CropType::Carrot; full.basePrice = 170; full.salePrice = 250;
		full.maturity = full.waterBalance = full.terrainFit = full.nutrientBalance = 1;
		full.nutrientKnown = true;
		assert(FarmCropQualitySystem::Analyze(full).focus == FarmQualityFocus::Balanced);
		constexpr std::array<FarmQualityFocus,4> focus{FarmQualityFocus::Maturity, FarmQualityFocus::Water,
			FarmQualityFocus::Terrain, FarmQualityFocus::Nutrients};
		for (std::size_t i=0; i<focus.size(); ++i) {
			auto low = full;
			std::array<float*,4> axes{&low.maturity, &low.waterBalance, &low.terrainFit, &low.nutrientBalance};
			*axes[i] = 0.58f;
			const auto advice = FarmCropQualitySystem::Analyze(low);
			assert(advice.focus == focus[i] && advice.lowestPercent == 58 && !advice.partial);
			assert(low.salePrice == 250 && *axes[i] == 0.58f);
		}
		auto tied = full; tied.waterBalance = 0.584f; tied.nutrientBalance = 0.581f;
		assert(FarmCropQualitySystem::Analyze(tied).focus == FarmQualityFocus::Water);
		auto partial = full; partial.nutrientKnown = false; partial.nutrientBalance = 0;
		assert(FarmCropQualitySystem::Analyze(partial).focus == FarmQualityFocus::Unknown);
		partial.waterBalance = 0.4f;
		assert(FarmCropQualitySystem::Analyze(partial).focus == FarmQualityFocus::Water);
		assert(FarmCropQualitySystem::Analyze(partial).partial);
		for (float invalid : {-1.0f, 2.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
			auto bad = full; bad.maturity = invalid;
			assert(FarmCropQualitySystem::Analyze(bad).focus == FarmQualityFocus::Unknown);
			assert(FarmCropQualitySystem::Analyze(bad).partial);
		}
		full.waterBalance = 0.999f;
		assert(FarmCropQualitySystem::Analyze(full).focus == FarmQualityFocus::Balanced);
		std::cout << "PASS: quality hints, displayed ties, unknown/invalid axes and read-only results\n";
	}
	{
		FarmCropQualitySystem quality;
		quality.Initialize();
		for (auto crop : {farm::CropType::TestCrop, farm::CropType::Carrot}) {
			farm::FarmTile full;
			full.state = farm::FarmTileState::Planted; full.crop = crop; full.growth = 1;
			full.soilNutrients = 1;
			auto split = full;
			FarmSoilSystem::Grow(full, 1);
			for (int i = 0; i < 100; ++i) FarmSoilSystem::Grow(split, 0.01f);
			assert(std::fabs(full.soilNutrients - split.soilNutrients) < 0.0001f);
			assert(std::fabs(full.careHistory.nutrientSupply - split.careHistory.nutrientSupply) < 0.0001f);
			auto poor = full; poor.careHistory = {}; poor.soilNutrients = 0.1f;
			FarmSoilSystem::Grow(poor, 1);
			assert(poor.soilNutrients == 0 && quality.Evaluate(poor).score < quality.Evaluate(full).score);
			assert(quality.Evaluate(poor).salePrice < quality.Evaluate(full).salePrice);
			assert(!FarmSoilSystem::ApplyCompost(poor));
		}
		assert(FarmSoilSystem::Satisfaction(0.5f, farm::CropType::TestCrop) >
			FarmSoilSystem::Satisfaction(0.5f, farm::CropType::Carrot));
		assert(FarmSoilSystem::Profile(static_cast<farm::CropType>(99)) == nullptr);
		farm::FarmGrid grid; assert(grid.Initialize(1, 1));
		FarmToolActionSystem tools; tools.Initialize();
		assert(!tools.CompostSelectedTile(grid));
		auto soil = *grid.GetSelectedTile(); soil.state = farm::FarmTileState::Tilled;
		assert(grid.SetTile(0, soil));
		assert(tools.CompostSelectedTile(grid));
		assert(grid.GetSelectedTile()->soilNutrients == 1);
		assert(!tools.CompostSelectedTile(grid));
		assert(tools.Undo() && grid.GetSelectedTile()->soilNutrients == soil.soilNutrients);
		assert(tools.Redo() && grid.GetSelectedTile()->soilNutrients == 1);
		soil.soilNutrients = std::numeric_limits<float>::quiet_NaN();
		assert(!grid.SetTile(0, soil));
		std::cout << "PASS: soil crop profiles, timestep independence, shortage quality and compost undo/redo\n";
	}
	{
		FarmCropQualitySystem quality;
		quality.Initialize();
		farm::FarmTile wellManaged;
		wellManaged.state = farm::FarmTileState::Planted;
		wellManaged.crop = farm::CropType::TestCrop;
		wellManaged.growth = 1.0f;
		wellManaged.moisture = 0.0f;
		wellManaged.careHistory.goodSeconds = 8.0f;
		wellManaged.careHistory.efficiencySeconds = 8.0f;
		farm::FarmTile poorlyManaged = wellManaged;
		poorlyManaged.careHistory = {};
		poorlyManaged.careHistory.drySeconds = 8.0f;
		poorlyManaged.careHistory.efficiencySeconds = 3.2f;
		const auto goodQuality = quality.Evaluate(wellManaged);
		const auto poorQuality = quality.Evaluate(poorlyManaged);
		assert(goodQuality.IsValid() && poorQuality.IsValid());
		assert(std::fabs(goodQuality.waterBalance - 1.0f) < 0.0001f);
		assert(std::fabs(poorQuality.waterBalance - 0.4f) < 0.0001f);
		assert(goodQuality.score > poorQuality.score &&
			goodQuality.salePrice > poorQuality.salePrice);
	}
	{
		farm::FarmGrid grid;
		FarmEconomySystem economy;
		FarmToolActionSystem actions;
		FarmGrowthSystem growth;
		assert(grid.Initialize(1, 1));
		economy.Initialize();
		actions.Initialize();
		growth.Initialize();
		const auto crop = farm::CropType::TestCrop;
		assert(actions.ApplyTool(grid, FarmTool::Hoe, crop, economy));
		assert(economy.BuySeed(crop).Succeeded());
		for (int dose = 0; dose < 5; ++dose) {
			assert(actions.ApplyTool(grid, FarmTool::Water, crop, economy));
		}
		assert(actions.ApplyTool(grid, FarmTool::Seed, crop, economy));
		grid.GetMutableSelectedTile()->moisture = 0.0f;
		assert(growth.Update(grid, 0.25f, 1.0f));
		grid.GetMutableSelectedTile()->moisture = 0.10f;
		assert(growth.Update(grid, 0.25f, 1.0f));
		grid.GetMutableSelectedTile()->moisture = 0.50f;
		assert(growth.Update(grid, 0.25f, 1.0f));
		grid.GetMutableSelectedTile()->moisture = 0.90f;
		assert(growth.Update(grid, 0.25f, 1.0f));
		const farm::FarmTile* growingTile = grid.GetSelectedTile();
		assert(growingTile != nullptr);
		assert(std::fabs(growingTile->careHistory.drySeconds - 0.25f) < 0.0001f);
		assert(std::fabs(growingTile->careHistory.lowSeconds - 0.25f) < 0.0001f);
		assert(std::fabs(growingTile->careHistory.goodSeconds - 0.25f) < 0.0001f);
		assert(std::fabs(growingTile->careHistory.excessSeconds - 0.25f) < 0.0001f);
		assert(growingTile->careHistory.efficiencySeconds > 0.0f &&
			growingTile->careHistory.efficiencySeconds <= 1.0f);
		farm::FarmTile readyTile = *growingTile;
		readyTile.growth = 1.0f;
		assert(grid.SetTile(0, readyTile));
		const auto expectedHistory = readyTile.careHistory;
		assert(readyTile.soilNutrients < 0.60f && expectedHistory.nutrientGrowth > 0);
		assert(actions.ApplyTool(grid, FarmTool::Harvest, crop, economy));
		assert(grid.GetSelectedTile()->careHistory.IsEmpty());
		assert(actions.Undo());
		assert(grid.GetSelectedTile()->soilNutrients == readyTile.soilNutrients);
		assert(grid.GetSelectedTile()->careHistory.nutrientSupply == expectedHistory.nutrientSupply);
		assert(grid.GetSelectedTile()->state == farm::FarmTileState::Planted);
		assert(std::fabs(grid.GetSelectedTile()->careHistory.goodSeconds -
			expectedHistory.goodSeconds) < 0.0001f);
		assert(actions.Redo());
		assert(grid.GetSelectedTile()->careHistory.IsEmpty());
	}
    {
        FarmEconomySystem economy;
        farm::FarmRules rules;
        rules.initialMoney = rules.testCropSeedPrice - 1;
        economy.Initialize(rules);
        assert(!economy.BuySeed(farm::CropType::TestCrop).Succeeded());
        assert(economy.GetMoney() == rules.initialMoney && economy.GetTotalSeedCount() == 0);
        rules.initialMoney = rules.testCropSeedPrice;
        economy.Initialize(rules);
        assert(economy.BuySeed(farm::CropType::TestCrop).Succeeded());
        assert(economy.GetMoney() == 0 && economy.GetTotalSeedCount() == 1);
        assert(!economy.BuySeed(farm::CropType::TestCrop).Succeeded());
        assert(economy.GetMoney() == 0 && economy.GetTotalSeedCount() == 1);
    }
	std::cout << "PASS: crop care history quality and harvest undo/redo\n";
    FarmPlayFlow flow;
    using Phase = FarmPlayFlow::Phase;
    assert(flow.BlocksSimulation());
    flow.Observe(false);
    assert(flow.GetPhase() == Phase::Briefing);
    flow.Continue(); flow.Continue();
    assert(flow.GetPhase() == Phase::Playing && !flow.BlocksSimulation());
    flow.Observe(true);
    assert(flow.GetPhase() == Phase::Result && flow.BlocksSimulation());
    flow.Continue();
    for (int i = 0; i < 10; ++i) flow.Observe(true);
    assert(flow.GetPhase() == Phase::Review && !flow.BlocksSimulation());
    flow.Observe(false);
    assert(flow.GetPhase() == Phase::Briefing);
    flow.Reset(); flow.Observe(true);
    assert(flow.GetPhase() == Phase::Result); // A loaded cleared farm is not a new challenge.

    for (const float speed : {1.0f, 2.0f, 4.0f}) {
        farm::FarmGrid grid;
        FarmEconomySystem economy;
        FarmToolActionSystem actions;
        FarmGrowthSystem growth;
        FarmProgressionSystem progression;
        assert(grid.Initialize(5, 4));
        economy.Initialize(); actions.Initialize(); growth.Initialize(); progression.Initialize();
        flow.Reset(); flow.Continue();
        const auto crop = farm::CropType::TestCrop;
        assert(actions.EvaluateTool(grid, FarmTool::Hoe, crop, &economy).Succeeded());
        assert(actions.ApplyTool(grid, FarmTool::Hoe, crop, economy));
        assert(!actions.EvaluateTool(grid, FarmTool::Hoe, crop, &economy).Succeeded());
        assert(actions.EvaluateTool(grid, FarmTool::Seed, crop, &economy).status == FarmToolActionStatus::NoSeed);
        assert(!actions.ApplyTool(grid, FarmTool::Seed, crop, economy));
        assert(!economy.SellAll().Succeeded());
        int harvests = 0;
        while (!progression.IsCleared() && harvests < 20) {
            const int moneyBeforePurchase = economy.GetMoney();
            const int seedsBeforePurchase = economy.GetSeedCount(crop);
            assert(economy.BuySeed(crop).Succeeded());
            assert(economy.GetMoney() == moneyBeforePurchase - farm::FarmRules{}.testCropSeedPrice);
            assert(economy.GetSeedCount(crop) == seedsBeforePurchase + 1);
            assert(grid.GetSelectedTile()->state == farm::FarmTileState::Tilled);
            assert(actions.EvaluateTool(grid, FarmTool::Seed, crop, &economy).Succeeded());
            while (grid.GetSelectedTile()->moisture < 0.5f)
                assert(actions.ApplyTool(grid, FarmTool::Water, crop, economy));
            assert(actions.ApplyTool(grid, FarmTool::Seed, crop, economy));
            assert(actions.EvaluateTool(grid, FarmTool::Harvest, crop, &economy).status == FarmToolActionStatus::NotReady);
            assert(economy.GetSeedCount(crop) == 0);
            for (int step = 0; step < 2000 && !farm::IsHarvestReady(*grid.GetSelectedTile()); ++step) {
                if (grid.GetSelectedTile()->moisture < 0.4f)
                    assert(actions.ApplyTool(grid, FarmTool::Water, crop, economy));
                assert(growth.Update(grid, 0.05f, speed));
            }
            assert(farm::IsHarvestReady(*grid.GetSelectedTile()));
            assert(actions.EvaluateTool(grid, FarmTool::Harvest, crop, &economy).Succeeded());
            assert(actions.ApplyTool(grid, FarmTool::Harvest, crop, economy));
            assert(!actions.EvaluateTool(grid, FarmTool::Harvest, crop, &economy).Succeeded());
            assert(!actions.ApplyTool(grid, FarmTool::Harvest, crop, economy));
            assert(economy.GetTotalCropCount() == 1);
            const int before = economy.GetMoney();
            const auto sale = economy.SellAll();
            assert(sale.Succeeded() && sale.soldCount == 1);
            assert(economy.GetMoney() == before + sale.earnedMoney);
            assert(economy.GetTotalCropCount() == 0 && !economy.SellAll().Succeeded());
            actions.ClearHistory(); // Same sale boundary as the Scene; no undo across committed sale.
            const bool justCleared = progression.EvaluateClear(economy.GetMoney());
            flow.Observe(progression.IsCleared());
            if (justCleared) {
                assert(!progression.EvaluateClear(economy.GetMoney()));
                assert(flow.GetPhase() == Phase::Result);
            }
            ++harvests;
        }
        assert(progression.IsCleared() && economy.GetMoney() >= progression.GetTargetMoney());
        flow.Continue(); flow.Observe(true);
        assert(flow.GetPhase() == Phase::Review);
        actions.ClearHistory();
        assert(grid.Initialize(5, 4));
        economy.Initialize(); progression.Initialize(); flow.Reset();
        assert(economy.GetMoney() == farm::FarmRules{}.initialMoney);
        assert(economy.GetTotalCropCount() == 0 && economy.GetTotalSeedCount() == 0);
        assert(!progression.IsCleared() && flow.BlocksSimulation());
        std::cout << "PASS: buy/grow/harvest/sell/clear/restart x" << speed << ", harvests=" << harvests << '\n';
    }
}
