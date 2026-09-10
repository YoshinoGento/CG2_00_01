#include "farm/ui/FarmRuntimeUI.h"
#include "farm/ui/FarmObservationView.h"
#include "farm/ui/FarmPlayFlowView.h"
#include "farm/ui/FarmFieldActionView.h"
#include "farm/ui/FarmCameraView.h"
#include "farm/ui/FarmQualityView.h"
#include "farm/ui/FarmTerrainView.h"
#include "farm/system/FarmOverviewCamera.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

int main() {
    using namespace farmui;
    for (bool preview : {false,true}) for (bool allowed : {false,true}) {
        TerrainViewState state;
        state.preview = preview; state.canConfirm = allowed;
        state.canRaise = state.canLower = state.canCanal = state.canSource = allowed;
        state.canPath = state.canUndo = state.canRedo = state.canCompost = allowed;
        state.tileValues = "639 / H3 / 100 / 100";
        state.changeCount = preview ? 640 : 0;
        View terrain;
        BuildTerrainView(terrain, state);
        assert(terrain.terrain && !terrain.modal && terrain.count == 17);
        assert(terrain.items[4].value == (preview ? "640" : "0"));
        assert(terrain.Hit({50,550}).action == (!preview && allowed ? Action::Raise : Action::None));
        assert(terrain.Hit({50,660}).action == (preview && allowed ? Action::Confirm : Action::None));
        assert(terrain.Hit({350,660}).action == (preview ? Action::Cancel : Action::None));
        assert(terrain.Hit({1050,40}).action == Action::TerrainExit);
        assert(terrain.Covers({20,530}) && terrain.Covers({335,550}) && terrain.Covers({20,108}));
        assert(!terrain.Covers({640,300}) && !terrain.Covers({640,519}));
        assert(terrain.Hit({335,550}).action == Action::None);
        assert(!terrain.Covers({1264,550}) && !terrain.Covers({640,704}));
        for (std::size_t i=0; i<terrain.count; ++i) {
            const auto& item = terrain.items[i];
            assert(item.rect.x >= 16 && item.rect.x+item.rect.width <= 1264);
            assert(item.rect.y >= 16 && item.rect.y+item.rect.height <= 704);
            const auto& uv = kLabels[static_cast<std::size_t>(item.label)];
            const float space = item.value.empty() ? item.rect.width-20 : item.valueOffset-30;
            assert(space/uv.width >= 20.0f/26.0f);
            if (!item.value.empty()) assert(item.valueOffset+14*(item.value.size()-1)+24 <= item.rect.width);
            for (std::size_t j=i+1; j<terrain.count; ++j) {
                const auto& other = terrain.items[j].rect;
                assert(item.rect.x+item.rect.width <= other.x || other.x+other.width <= item.rect.x ||
                    item.rect.y+item.rect.height <= other.y || other.y+other.height <= item.rect.y);
            }
        }
        View quick;
        BuildPlayQuickView(quick, preview, allowed);
        assert(quick.Hit({400,40}).action == (allowed ? Action::TerrainField : Action::None));
        assert(quick.Hit({700,40}).action == Action::Pause);
        assert(quick.items[1].label == (preview ? Label::Play : Label::Pause));
        for (std::size_t i=0; i<quick.count; ++i) {
            const auto& rect = quick.items[i].rect;
            assert(!View::FarmHUDCovers({rect.x,rect.y}));
            assert(!View::FarmHUDCovers({rect.x+rect.width-1,rect.y+rect.height-1}));
        }
    }
    for (bool harvested : {false,true}) {
        View empty;
        BuildQualityView(empty, {}, {}, -1, harvested);
        assert(!empty.radar.visible && empty.count == 3);
        assert(empty.Hit({700,150}).action == Action::Quality);
        FarmCropQualityResult quality;
        quality.crop = farm::CropType::Carrot; quality.basePrice = 170; quality.salePrice = 210;
        quality.score = 85; quality.maturity = 0.75f; quality.waterBalance = 1;
        quality.terrainFit = 0.5f; quality.nutrientBalance = 0.25f; quality.nutrientKnown = true;
        View chart;
        BuildQualityView(chart, quality, FarmCropQualitySystem::Analyze(quality), 12, harvested);
        assert(chart.items[chart.count-1].label == Label::HintNutrients);
        assert(chart.radar.visible && chart.radar.known[3] && chart.radar.values[3] == 0.25f);
        assert(chart.count + 7 < View::kCapacity);
        assert(chart.items[0].selected != harvested && chart.items[1].selected == harvested);
        for (std::size_t i=0; i<chart.count; ++i) {
            const auto& item = chart.items[i];
            assert(item.rect.x >= 150 && item.rect.x+item.rect.width <= 1130);
            assert(item.rect.y >= 130 && item.rect.y+item.rect.height <= 618);
            const auto& uv = kLabels[static_cast<std::size_t>(item.label)];
            const float space = item.value.empty() ? item.rect.width-20 : item.valueOffset-30;
            assert(space/uv.width >= 20.0f/26.0f);
            if (!item.value.empty()) assert(item.valueOffset+14*(item.value.size()-1)+24 <= item.rect.width);
            for (std::size_t j=i+1; j<chart.count; ++j) {
                const auto& other = chart.items[j].rect;
                assert(item.rect.x+item.rect.width <= other.x || other.x+other.width <= item.rect.x ||
                    item.rect.y+item.rect.height <= other.y || other.y+other.height <= item.rect.y);
            }
        }
        quality.nutrientKnown = false;
        quality.waterBalance = std::numeric_limits<float>::quiet_NaN();
        quality.terrainFit = 3;
        View old;
        BuildQualityView(old, quality, FarmCropQualitySystem::Analyze(quality), -1, harvested);
        assert(old.items[old.count-3].label == (harvested ? Label::HintPartialRecord : Label::HintPartialEstimate));
        assert(!old.radar.known[3] && !old.radar.known[1] && old.radar.values[2] == 1);
    }
    assert(QualityRadar::Point(0,1).y == QualityRadar::center.y-QualityRadar::radius);
    assert(QualityRadar::Point(1,2).x == QualityRadar::center.x+QualityRadar::radius);
    assert(QualityRadar::Point(3,-1).x == QualityRadar::center.x);
    assert(QualityRadar::Point(99,1).x == QualityRadar::center.x);
    assert(QualityRadar::Point(0,std::numeric_limits<float>::infinity()).y == QualityRadar::center.y);
    for (auto focus : {FarmQualityFocus::Maturity, FarmQualityFocus::Water, FarmQualityFocus::Terrain,
        FarmQualityFocus::Nutrients, FarmQualityFocus::Balanced, FarmQualityFocus::Unknown}) {
        for (bool harvested : {false,true}) {
            const auto& uv = kLabels[static_cast<std::size_t>(QualityHintLabel(focus,harvested))];
            assert(uv.width <= 900 && uv.height <= 38);
        }
    }
    assert(View::FarmHUDCovers({24,24}) && View::FarmHUDCovers({333.99f,155.99f}));
    assert(!View::FarmHUDCovers({334,24}) && !View::FarmHUDCovers({24,156}));
    assert(View::FarmHUDCovers({876,24}) && View::FarmHUDCovers({1255.99f,251.99f}));
    assert(View::FarmHUDCovers({24,520}) && View::FarmHUDCovers({390,548}));
    assert(!View::FarmHUDCovers({640,300}) && !View::FarmHUDCovers({1279,719}));
    for (const auto frame : {farm::kFarmOverviewFrame, farm::kObservationOverviewFrame, farm::kTerrainOverviewFrame}) {
        for (float aspect : {4.0f/3, 16.0f/9, 21.0f/9}) {
            for (float extent : {0.6f, 3.5f, 12.0f}) {
                for (float height : {0.0f, 1.16f, 4.0f}) {
                    const Vector3 minimum{-extent, 0.05f, 8-extent};
                    const Vector3 maximum{extent, 0.05f+height, 8+extent};
                    const float py = 1/std::tan(0.45f/2), px = py/aspect;
                    farm::OverviewPose pose;
                    assert(farm::FitOverviewCamera(minimum, maximum, px, py, 0.1f, 1000, frame, pose));
                    for (int corner = 0; corner < 8; ++corner) {
                        const float dx = ((corner&1) ? maximum.x : minimum.x)-pose.position.x;
                        const float dy = ((corner&2) ? maximum.y : minimum.y)-pose.position.y;
                        const float dz = ((corner&4) ? maximum.z : minimum.z)-pose.position.z;
                        const float cameraY = std::cos(pose.rotation.x)*dy+std::sin(pose.rotation.x)*dz;
                        const float cameraZ = -std::sin(pose.rotation.x)*dy+std::cos(pose.rotation.x)*dz;
                        const float screenX = (px*dx/cameraZ+1)*0.5f;
                        const float screenY = (1-py*cameraY/cameraZ)*0.5f;
                        assert(cameraZ > 0.1f && cameraZ < 1000);
                        assert(screenX >= frame.left-0.0001f && screenX <= frame.right+0.0001f);
                        assert(screenY >= frame.top-0.0001f && screenY <= frame.bottom+0.0001f);
                    }
                }
            }
        }
    }
    farm::OverviewPose unchanged{{123,456,789}, {0,0,0}};
    for (float invalid : {0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        assert(!farm::FitOverviewCamera({0,0,0}, {1,1,1}, invalid, 4, 0.1f, 100, farm::kFarmOverviewFrame, unchanged));
    assert(!farm::FitOverviewCamera({2,0,0}, {1,1,1}, 2, 4, 0.1f, 100, farm::kFarmOverviewFrame, unchanged));
    assert(!farm::FitOverviewCamera({0,0,0}, {1,1,1}, 2, 4, 0.1f, 1, farm::kFarmOverviewFrame, unchanged));
    assert(!farm::FitOverviewCamera({0,0,0}, {1,1,1}, 2, 4, 0.1f, 100, {0,0,0,1}, unchanged));
    assert(unchanged.position.x == 123 && unchanged.position.y == 456 && unchanged.position.z == 789);
    for (bool follow : {false,true}) for (bool available : {false,true}) {
        View camera;
        BuildCameraView(camera, follow, available);
        assert(camera.Hit({1100,450}).action == Action::Overview);
        assert(camera.Hit({1100,500}).action == (available ? Action::Follow : Action::None));
        assert(camera.Covers({1100,500}) && !camera.Covers({500,300}));
        assert(camera.items[1].selected == follow);
        for (std::size_t i=0; i<camera.count; ++i) {
            const auto& item = camera.items[i];
            const auto& uv = kLabels[static_cast<std::size_t>(item.label)];
            assert((item.rect.width-20)/uv.width >= 20.0f/26.0f);
            assert(item.rect.y+item.rect.height < View::kFieldActionsPanel.y);
        }
    }
    View view;
    for (int target : {-1, 0, 19, 639}) {
        FarmToolActionResult evaluation;
        evaluation.tileIndex = target;
        evaluation.status = target < 0 ? FarmToolActionStatus::InvalidTarget : FarmToolActionStatus::Applied;
        View targetView;
        BuildFieldActionView(targetView, evaluation, false, false);
        const auto& metric = targetView.items[0];
        assert(metric.label == Label::Selected && metric.request.action == Action::None);
        assert(metric.value == (target < 0 ? "--" : std::to_string(target)));
        assert(targetView.Covers({1100,550}) && targetView.Hit({1100,550}).action == Action::None);
        assert(metric.valueOffset + 14*(metric.value.size()-1) + 24 <= metric.rect.width);
        const auto& uv = kLabels[static_cast<std::size_t>(metric.label)];
        assert((metric.valueOffset-30)/uv.width >= 20.0f/26.0f);
        evaluation.status = FarmToolActionStatus::InvalidTarget;
        View invalid;
        BuildFieldActionView(invalid, evaluation, false, false);
        assert(invalid.items[0].value == "--");
    }
    for (const auto next : {FarmHUDNextAction::Hoe, FarmHUDNextAction::WaterOrSeed,
        FarmHUDNextAction::Seed, FarmHUDNextAction::Water, FarmHUDNextAction::Harvest, FarmHUDNextAction::BuySeed,
        FarmHUDNextAction::Growing, FarmHUDNextAction::ReduceWater, FarmHUDNextAction::Canal,
        FarmHUDNextAction::WaterSource, FarmHUDNextAction::SelectTile}) {
        for (int blocked = 0; blocked < 4; ++blocked) {
            View field;
            FarmToolActionResult evaluation;
            evaluation.status = FarmToolActionStatus::InvalidState;
            evaluation.tool = next == FarmHUDNextAction::Hoe ? FarmTool::Seed : FarmTool::Hoe;
            BuildFieldActionView(field, evaluation, (blocked & 1) != 0, (blocked & 2) != 0, next);
            const auto hit = field.Hit({1100, 610});
            const int expectedTool = next == FarmHUDNextAction::Hoe ? 0 :
                (next == FarmHUDNextAction::Water || next == FarmHUDNextAction::WaterOrSeed) ? 1 :
                next == FarmHUDNextAction::Seed ? 2 : next == FarmHUDNextAction::Harvest ? 3 : -1;
            if (blocked) assert(hit.action == Action::None);
            else if (expectedTool >= 0) { assert(hit.action == Action::Tool && hit.argument == expectedTool); }
            else assert(hit.action == (next == FarmHUDNextAction::BuySeed ? Action::OpenSeedShop : Action::None));
            assert(hit.action != Action::ApplyTool); // A next-tool shortcut never farms implicitly.
            assert(field.Covers({1100, 610}));
            for (std::size_t i = 0; i < field.count; ++i) {
                const auto& item = field.items[i];
                const auto& uv = kLabels[static_cast<std::size_t>(item.label)];
                assert((item.rect.width - 20) / uv.width >= 20.0f / 26.0f);
            }
        }
    }
    View sameTool;
    FarmToolActionResult unavailable;
    unavailable.status = FarmToolActionStatus::InvalidState;
    unavailable.tool = FarmTool::Hoe;
    BuildFieldActionView(sameTool, unavailable, false, false, FarmHUDNextAction::Hoe);
    assert(sameTool.Hit({1100, 610}).action == Action::None);
    for (const auto status : {FarmToolActionStatus::None, FarmToolActionStatus::Applied,
        FarmToolActionStatus::Harvested, FarmToolActionStatus::InvalidTarget, FarmToolActionStatus::InvalidState,
        FarmToolActionStatus::AlreadyWatered, FarmToolActionStatus::NoSeed, FarmToolActionStatus::NotReady,
        FarmToolActionStatus::UnsupportedTool}) {
        for (const auto tool : {FarmTool::Hoe, FarmTool::Water, FarmTool::Seed, FarmTool::Harvest}) {
            for (int blocked = 0; blocked < 4; ++blocked) {
                View field;
                FarmToolActionResult evaluation;
                evaluation.status = status; evaluation.tool = tool;
                BuildFieldActionView(field, evaluation, (blocked & 1) != 0, (blocked & 2) != 0);
                field.Add(Label::Menu, {1040, 654, 216, 42}, {Action::Menu});
                const auto expected = blocked != 0 ? Action::None : status == FarmToolActionStatus::NoSeed
                    ? Action::OpenSeedShop : evaluation.Succeeded() ? Action::ApplyTool : Action::None;
                assert(field.Hit({1100, 610}).action == expected);
                assert(field.Covers({1100, 610}) && field.Covers({1042, 542}));
                assert(field.Hit({1100, 558}).action == Action::None);
                assert(field.Hit({1100, 670}).action == Action::Menu);
                assert(!field.Covers({640, 300}));
                for (std::size_t i = 0; i < field.count; ++i) {
                    const auto& a = field.items[i].rect;
                    assert(a.x >= 1020 && a.x+a.width <= 1256 && a.y >= 532 && a.y+a.height <= 696);
                    const auto& uv = kLabels[static_cast<std::size_t>(field.items[i].label)];
                    // Keep the source 26px Japanese font at least 20px at 1280x720.
                    assert((a.width-20) / uv.width >= 20.0f/26.0f);
                    for (std::size_t j = i+1; j < field.count; ++j) {
                        const auto& b = field.items[j].rect;
                        assert(a.x+a.width <= b.x || b.x+b.width <= a.x || a.y+a.height <= b.y || b.y+b.height <= a.y);
                    }
                }
            }
        }
    }
    for (const auto phase : {FarmPlayFlow::Phase::Briefing, FarmPlayFlow::Phase::Result}) {
        View entry;
        FarmHUDViewData data;
        data.money = 300; data.goalMoney = 540;
        BuildPlayFlowView(entry, phase, data);
        assert(entry.modal && entry.Covers({640, 300}));
        assert(entry.Hit({200, 510}).action == Action::FlowContinue);
        assert(entry.Hit({700, 510}).action == Action::Restart);
        assert(entry.Hit({200, 570}).action == Action::FlowRecords);
        for (std::size_t i = 0; i < entry.count; ++i) {
            const auto& a = entry.items[i].rect;
            assert(a.x >= 0 && a.x+a.width <= 1280 && a.y >= 0 && a.y+a.height <= 688);
            for (std::size_t j = i+1; j < entry.count; ++j) {
                const auto& b = entry.items[j].rect;
                assert(a.x+a.width <= b.x || b.x+b.width <= a.x || a.y+a.height <= b.y || b.y+b.height <= a.y);
            }
        }
    }
    assert(view.Hit({0, 0}).action == Action::None);
    assert(view.NextActionable(-1, 1) == -1);
    view.Add(Label::Menu, {10, 20, 100, 40}, {Action::Menu});
    view.Add(Label::Save, {120, 20, 100, 40}, {Action::Save}, false);
    view.Add(Label::Ready, {10, 80, 100, 40});
    view.Add(Label::Cancel, {120, 80, 100, 40}, {Action::Cancel});
    assert(view.Hit({10, 20}).action == Action::Menu);
    assert(view.Hit({110, 20}).action == Action::None);
    assert(view.Hit({10, 60}).action == Action::None);
    assert(view.Hit({130, 30}).action == Action::None);
    assert(view.Covers({130, 30})); // Disabled buttons must not click through.
    assert(view.Covers({20, 90})); // Noninteractive labels also own their area.
    assert(!view.Covers({500, 500}));
    assert(view.Hit({std::numeric_limits<float>::quiet_NaN(), 20}).action == Action::None);
    assert(view.Hit({std::numeric_limits<float>::infinity(), 20}).action == Action::None);
    assert(view.NextActionable(-1, 1) == 0);
    assert(view.NextActionable(0, 1) == 3);
    assert(view.NextActionable(3, 1) == 0);
    assert(view.NextActionable(0, -1) == 3);
    view.Add(Label::Accept, {10, 20, 100, 40}, {Action::Accept});
    assert(view.Hit({20, 30}).action == Action::Accept);
    for (int i = 0; i < 100; ++i) view.Add(Label::Menu, {}, {Action::Menu});
    assert(view.count == View::kCapacity);
    view.count = 1000;
    assert(view.Covers({130, 30}));
    assert(view.Hit({20, 30}).action == Action::Accept);
    for (auto& item : view.items) item.enabled = false;
    assert(view.NextActionable(1, 1) == -1);
    assert(view.Hit({20, 30}).action == Action::None);
    for (const auto& uv : kLabels) {
        assert(std::isfinite(uv.x) && std::isfinite(uv.y));
        assert(uv.width > 0 && uv.height > 0);
        assert(uv.x >= 0 && uv.x + uv.width <= 1280);
        assert(uv.y >= 0 && uv.y + uv.height <= kAsciiY);
    }
    FarmComparisonView comparison{};
    for (const auto status : {FarmComparisonStatus::Idle, FarmComparisonStatus::Running,
        FarmComparisonStatus::Stopped, FarmComparisonStatus::Completed, FarmComparisonStatus::Invalidated}) {
        for (int picking = -1; picking < 2; ++picking) {
            View observation;
            comparison.status = status;
            comparison.startIssue = FarmComparisonIssue::None;
            comparison.rows[0].tileIndex = 12;
            comparison.rows[1].tileIndex = 13;
            BuildObservationView(observation, comparison, {true, false}, picking, 12, true, 2, true);
            assert(observation.count < View::kCapacity && observation.observation && !observation.modal);
            assert(observation.Covers({30, 30}) && observation.Covers({1250, 680}));
            assert(!observation.Covers({640, 300}));
            assert(observation.Hit({400, 445}).action == Action::JumpSlot);
            assert(observation.Hit({1000, 445}).action == Action::None);
            assert((observation.Hit({60, 516}).action == Action::PickSlot) == (status != FarmComparisonStatus::Running));
            assert((observation.Hit({60, 626}).action == Action::Start) == (status != FarmComparisonStatus::Running));
            assert((observation.Hit({300, 626}).action == Action::Stop) == (status == FarmComparisonStatus::Running));
            for (std::size_t i = 0; i < observation.count; ++i) {
                const auto& r = observation.items[i].rect;
                assert(r.x >= 0 && r.y >= 0 && r.x+r.width <= 1280 && r.y+r.height <= 688);
                for (std::size_t j = i+1; j < observation.count; ++j) {
                    const auto& b = observation.items[j].rect;
                    assert(r.x+r.width <= b.x || b.x+b.width <= r.x || r.y+r.height <= b.y || b.y+b.height <= r.y);
                }
            }
        }
    }
    for (const auto issue : {FarmComparisonIssue::PickTwo, FarmComparisonIssue::SameTile, FarmComparisonIssue::NotGrowing,
        FarmComparisonIssue::Crop, FarmComparisonIssue::Height, FarmComparisonIssue::Growth, FarmComparisonIssue::InvalidData}) {
        comparison.status = FarmComparisonStatus::Idle; comparison.startIssue = issue;
        View observation;
        BuildObservationView(observation, comparison, {false, false}, -1, -1, true, 1, false);
        assert(observation.Hit({60, 626}).action == Action::None);
        assert(ComparisonIssueLabel(issue) != Label::CompareReady);
    }
    std::cout << "PASS: hit boundaries, disabled actions, navigation, capacity, atlas bounds\n";
}
