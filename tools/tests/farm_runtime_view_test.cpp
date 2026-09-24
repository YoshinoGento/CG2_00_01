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
    const auto separated = [](Rect a, Rect b) {
        return a.x+a.width<=b.x || b.x+b.width<=a.x || a.y+a.height<=b.y || b.y+b.height<=a.y;
    };
    for (const auto grade : {FarmContestRating::Unrated,FarmContestRating::D,FarmContestRating::C,
        FarmContestRating::B,FarmContestRating::A,FarmContestRating::S}) {
        FarmContestSeasonSummary summary{};
        summary.valid=true; summary.finalized=true; summary.rating=grade;
        summary.submitted=grade==FarmContestRating::Unrated ? 0 : 3;
        summary.nextRating=grade==FarmContestRating::Unrated || grade==FarmContestRating::S ?
            FarmContestRating::Unrated : FarmContestRating::S;
        summary.pointsToNextRating=120;
        View result;
        BuildSeasonEndView(result,{},summary);
        bool found=false;
        for(std::size_t i=0;i<result.count;++i) {
            const auto& item=result.items[i];
            if(item.label==Label::SeasonRating || item.label==Label::SeasonUnrated) {
                found=true; assert(item.value==FarmContestRatingText(grade));
                assert((item.label==Label::SeasonUnrated)==(grade==FarmContestRating::Unrated));
            }
            const float space=item.value.empty() ? item.rect.width-20 : item.valueOffset-30;
            assert(space/kLabels[static_cast<std::size_t>(item.label)].width>=20.f/26.f);
            if(!item.value.empty()) assert(item.valueOffset+14*(item.value.size()-1)+24<=item.rect.width);
            for(std::size_t j=i+1;j<result.count;++j) assert(separated(item.rect,result.items[j].rect));
        }
        assert(found);
    }
    {
        FarmEconomySystem::ContestResults results{};
        View end; BuildSeasonEndView(end,results,FarmContestSeasonSystem::Evaluate(31,results));
        assert(end.modal && end.Hit({900,150}).action==Action::FlowRecords);
        assert(end.Hit({400,640}).action==Action::FlowContinue);
        assert(end.Hit({800,640}).action==Action::Restart);
        for(std::size_t i=0;i<end.count;++i) {
            const auto& a=end.items[i];
            assert(a.rect.y+a.rect.height<=688);
            for(std::size_t j=i+1;j<end.count;++j) assert(separated(a.rect,end.items[j].rect));
        }
        for(bool season : {false,true}) {
            View entry; FarmHUDViewData data; data.contestSeason=season;
            BuildPlayFlowView(entry,FarmPlayFlow::Phase::Briefing,data);
            const auto change=entry.Hit({850,570});
            assert(change.action==Action::ChangePlayMode && change.argument==(season ? 0 : 1));
            for(std::size_t i=0;i<entry.count;++i) {
                const auto& a=entry.items[i];
                const float space=a.value.empty() ? a.rect.width-20 : a.valueOffset-30;
                assert(space/kLabels[static_cast<std::size_t>(a.label)].width>=20.f/26.f);
                for(std::size_t j=i+1;j<entry.count;++j) assert(separated(a.rect,entry.items[j].rect));
            }
        }
    }
    for(int day : {10,20,30}) {
        View view; BuildContestDayNoticeView(view,day);
        assert(view.modal && view.count==7);
        assert(view.Hit({400,445}).action==Action::ContestDayReview);
        assert(view.Hit({800,445}).action==Action::ContestDayPrepare);
        assert(view.Hit({400,520}).action==Action::ContestDayResume);
        assert(view.Hit({400,445}).argument==day);
        assert(view.Covers({500,300}) && view.Hit({500,300}).action==Action::None);
        for(std::size_t i=0;i<view.count;++i) {
            const auto& item=view.items[i]; const auto& r=item.rect;
            assert(r.x>=150 && r.x+r.width<=1130 && r.y>=120 && r.y+r.height<=616);
            const float space=item.value.empty() ? r.width-20 : item.valueOffset-30;
            assert(space/kLabels[static_cast<std::size_t>(item.label)].width>=20.f/26.f);
            for(std::size_t j=i+1;j<view.count;++j) assert(separated(r,view.items[j].rect));
        }
    }
    for (int message=0; message<=static_cast<int>(FarmHUDFeedback::InsufficientMoney); ++message)
        for (int protectedCount : {0, 2, 2147483647})
            for (auto crop : {farm::CropType::None, farm::CropType::TestCrop, farm::CropType::Carrot})
                for (bool planted : {false, true}) {
        const bool feedback = static_cast<FarmHUDFeedback>(message) != FarmHUDFeedback::None;
        const FarmCropSizeResult size{1.5f, true};
        View view;
        BuildPlayQuickView(view, false, true);
        BuildPlayCropStatusView(view, feedback, protectedCount, crop, planted ? &size : nullptr);
        const std::size_t metrics = (planted ? 1u : 0u) + (protectedCount > 0 ? 1u : 0u) +
            (crop != farm::CropType::None ? 1u : 0u);
        assert(view.feedback == feedback && view.count == 5 + (feedback ? 0 : metrics));
        const auto panel = View::kFeedbackPanel;
        assert(panel.x>=0 && panel.y>=0 && panel.x+panel.width<=1280 && panel.y+panel.height<=720);
        for (const auto& other : View::kFarmHudPanels) assert(separated(panel, other));
        for (std::size_t i=0; i<view.count; ++i) {
            if (feedback) assert(separated(panel, view.items[i].rect));
            for (std::size_t j=i+1; j<view.count; ++j) assert(separated(view.items[i].rect,view.items[j].rect));
        }
        if (feedback) {
            assert(view.Covers({panel.x,panel.y}));
            assert(view.Covers({panel.x+panel.width-1,panel.y+panel.height-1}));
            assert(!view.Covers({panel.x+panel.width,panel.y+panel.height}));
            assert(view.Hit({panel.x+10,panel.y+10}).action==Action::None);
            assert(view.Hit({400,40}).action==Action::TerrainField);
            assert(view.Hit({650,40}).action==Action::Pause);
        }
        // Each refresh starts a new View, restoring current data rather than cached pre-toast values.
        view = {};
        BuildPlayCropStatusView(view, false, 3, farm::CropType::Carrot, &size);
        assert(!view.feedback && view.count==3);
        assert(view.items[0].label==Label::SizeForecast && view.items[0].value=="1.50x");
        assert(view.items[1].label==Label::ProtectedCropCount && view.items[1].value=="3");
        assert(view.items[2].label==Label::ContestCarrot);
    }
    for (const auto issue : {FarmContestJudgeIssue::None, FarmContestJudgeIssue::NoReservation,
        FarmContestJudgeIssue::InvalidRecord, FarmContestJudgeIssue::InvalidRules}) {
        FarmContestJudgeResult result; result.issue=issue; result.crop=farm::CropType::Carrot;
        result.recordedQuality=100; result.recordedSize=8; result.qualityPoints=60; result.sizePoints=40; result.totalPoints=100;
        View view; BuildContestJudgeView(view,result);
        assert(view.Hit({800,150}).action==Action::ContestPreview && view.Hit({800,150}).argument==1);
        assert(view.Hit({800,200}).action==Action::HarvestInventory);
        std::size_t glyphs=0;
        bool total=false;
        for (std::size_t i=0;i<view.count;++i) {
            const auto& item=view.items[i]; const auto& r=item.rect;
            total |= item.label==Label::ContestTotal;
            assert(r.x>=150 && r.x+r.width<=1130 && r.y>=120 && r.y+r.height<=616);
            const float space=item.value.empty() ? r.width-20 : item.valueOffset-30;
            assert(space/kLabels[static_cast<std::size_t>(item.label)].width>=20.f/26.f);
            if(!item.value.empty()) assert(item.valueOffset+14*(item.value.size()-1)+24<=r.width);
            glyphs+=item.value.size();
            for(std::size_t j=i+1;j<view.count;++j) {
                const auto& other=view.items[j].rect;
                assert(r.x+r.width<=other.x || other.x+other.width<=r.x || r.y+r.height<=other.y || other.y+other.height<=r.y);
            }
        }
        assert(total==result.IsValid() && glyphs<=160 && view.count+7<=View::kCapacity);
    }
    for (auto issue : {FarmContestEntryIssue::Eligible, FarmContestEntryIssue::InvalidDay,
        FarmContestEntryIssue::SeasonEnded, FarmContestEntryIssue::NoReservation, FarmContestEntryIssue::InvalidRecord,
        FarmContestEntryIssue::UnknownHarvestDay, FarmContestEntryIssue::FutureHarvestDay, FarmContestEntryIssue::OutsidePeriod}) {
        FarmContestEntryResult result; result.issue=issue; result.currentDay=2147483647;
        result.contestDay=20; result.firstHarvestDay=11; result.daysRemaining=9;
        result.harvestedDay=2147483647; result.crop=farm::CropType::Carrot;
        View view; BuildContestEntryView(view,result);
        assert(view.Hit({800,150}).action==Action::ContestPreview && view.Hit({800,150}).argument==0);
        assert(view.Hit({800,380}).action==Action::HarvestInventory);
        std::size_t glyphs=0;
        for(std::size_t i=0;i<view.count;++i) {
            const auto& item=view.items[i]; const auto& r=item.rect;
            assert(r.x>=150 && r.x+r.width<=1130 && r.y>=120 && r.y+r.height<=616);
            const float space=item.value.empty() ? r.width-20 : item.valueOffset-30;
            assert(space/kLabels[static_cast<std::size_t>(item.label)].width>=20.f/26.f);
            if(!item.value.empty()) assert(item.valueOffset+14*(item.value.size()-1)+24<=r.width);
            glyphs+=item.value.size();
            for(std::size_t j=i+1;j<view.count;++j) assert(separated(r,view.items[j].rect));
        }
        assert(glyphs<=160 && view.count+7<=View::kCapacity);
    }
    for (auto status : {FarmContestSubmissionStatus::Ready, FarmContestSubmissionStatus::NotContestDay,
        FarmContestSubmissionStatus::AlreadySubmitted, FarmContestSubmissionStatus::Ineligible}) {
        View view; FarmContestEntryResult entry;
        BuildContestEntryView(view,entry,status,{Action::SubmitContest,7,9});
        const auto hit=view.Hit({400,532});
        assert(hit.action==(status==FarmContestSubmissionStatus::Ready ? Action::SubmitContest : Action::None));
        if(status==FarmContestSubmissionStatus::Ready) assert(hit.argument==7 && hit.inventoryGeneration==9);
        assert(view.Hit({800,532}).action==Action::ContestResults);
    }
    for (int day : {0,1,10,11,20,21,30,31,2147483647}) for (bool populated : {false,true}) {
        FarmEconomySystem::ContestResults results{};
        if(populated) for(std::size_t i=0; i<results.size(); ++i) {
            auto& result=results[i]; result.contestDay=10+static_cast<int>(i)*10;
            result.harvest.quality.crop=farm::CropType::Carrot;
            result.harvest.harvestedDay=result.contestDay;
            result.harvest.quality.harvestSize={8,true}; result.qualityPoints=60; result.sizePoints=40;
        }
        const auto summary=FarmContestSeasonSystem::Evaluate(day,results);
        View view; BuildContestResultsView(view,results,summary);
        assert(view.items[0].label==(summary.finalized ? Label::ContestSeasonFinal : Label::ContestResults));
        assert(view.Hit({900,150}).action==Action::ContestPreview);
        if (!populated && day>0) {
            for(std::size_t i=0; i<3; ++i)
                assert(view.items[3+i].label==ContestEventLabel(summary.events[i]));
        }
        std::size_t glyphs=0;
        for(std::size_t i=0;i<view.count;++i) {
            const auto& item=view.items[i]; const auto& r=item.rect;
            assert(r.x>=150 && r.x+r.width<=1130 && r.y>=120 && r.y+r.height<=616);
            const float space=item.value.empty() ? r.width-20 : item.valueOffset-30;
            assert(space/kLabels[static_cast<std::size_t>(item.label)].width>=20.f/26.f);
            if(!item.value.empty()) assert(item.valueOffset+14*(item.value.size()-1)+24<=r.width);
            glyphs+=item.value.size();
            for(std::size_t j=i+1;j<view.count;++j) assert(separated(r,view.items[j].rect));
        }
        assert(glyphs<=160 && view.count+7<=View::kCapacity);
    }
    for (int day : {-1, 0, 31, 2147483647}) {
        FarmContestEntryResult result; result.currentDay=day;
        result.issue=day<1 ? FarmContestEntryIssue::InvalidDay : FarmContestEntryIssue::SeasonEnded;
        View view; BuildContestEntryView(view,result);
        assert(view.items[3].value=="--" && view.items[4].value=="--" && view.items[5].value=="--");
    }
    for (int count : {0, 1, 2, 3, 128}) for (bool unknown : {false, true}) for(int day : {0,1,10,20,30,2147483647}) {
        HarvestInventoryViewState state;
        state.records = count; state.capacity = 128; state.rowCount = (std::min)(count, state.kRows);
        state.pages = (std::max)(1, (count + state.kRows - 1) / state.kRows); state.unknownCount = unknown ? 2147483647 : 0;
        state.inventoryGeneration = 9; state.canChangeProtection = true;
        int nextId = 101;
        for (auto& row : state.rows) {
            row.id = nextId++;
            row.quality.crop = farm::CropType::Carrot; row.quality.harvestSize = {2, true};
            row.quality.score = 100; row.quality.salePrice = 2147483647; row.quantity = 2147483647;
            row.harvestedDay = day;
        }
        View view; BuildHarvestInventoryView(view, state);
        assert(view.Hit({500,550}).action==Action::ContestPreview);
        for (int i=0; i<state.rowCount; ++i) {
            const auto request = view.Hit({250.f, 300.f+i*88.f});
            assert(request.action == Action::ProtectHarvest && request.argument == 101+i && request.inventoryGeneration==9);
        }
        assert(view.count < View::kCapacity);
        assert(view.Hit({200,450}).action == Action::None);
        assert(view.Hit({800,450}).action == (state.pages > 1 ? Action::HarvestPage : Action::None));
        std::size_t glyphs = 0;
        int dateLabels = 0;
        for (std::size_t i=0; i<view.count; ++i) {
            const auto& item = view.items[i]; const auto& rect = item.rect;
            if(item.label == Label::HarvestDay || item.label == Label::HarvestDayUnknown) {
                ++dateLabels;
                assert(day > 0 ? item.value == std::to_string(day) : item.label == Label::HarvestDayUnknown);
                assert(item.request.action == Action::None);
            }
            assert(rect.x >= 150 && rect.x+rect.width <= 1130 && rect.y >= 120 && rect.y+rect.height <= 616);
            const float space = item.value.empty() ? rect.width-20 : item.valueOffset-30;
            assert(space/kLabels[static_cast<std::size_t>(item.label)].width >= 20.f/26.f);
            if (!item.value.empty()) assert(item.valueOffset + 14*(item.value.size()-1) + 24 <= rect.width);
            glyphs += item.value.size();
            for (std::size_t j=i+1; j<view.count; ++j) {
                const auto& other = view.items[j].rect;
                assert(rect.x+rect.width <= other.x || other.x+other.width <= rect.x ||
                    rect.y+rect.height <= other.y || other.y+other.height <= rect.y);
            }
        }
        assert(glyphs <= 160);
        assert(dateLabels == state.rowCount);
    }
    {
        HarvestInventoryViewState state; state.records=state.rowCount=1; state.rows[0].id=42;
        state.rows[0].saleProtected=true; state.inventoryGeneration=7;
        for (bool enabled : {false,true}) {
            state.canChangeProtection=enabled;
            View view; BuildHarvestInventoryView(view,state);
            const auto request=view.Hit({250,300});
            assert(request.action==(enabled ? Action::UnprotectHarvest : Action::None));
            if(enabled) assert(request.argument==42 && request.inventoryGeneration==7);
        }
    }
    for (bool reserved : {false,true}) for (bool eligible : {false,true}) for (bool enabled : {false,true}) {
        HarvestInventoryViewState state; state.records=state.rowCount=1; state.rows[0].id=42;
        state.rows[0].saleProtected=true; state.rows[0].canReserve=eligible; state.inventoryGeneration=77;
        state.contestReservationId=reserved ? 42 : 0; state.canChangeProtection=enabled;
        View view; BuildHarvestInventoryView(view,state);
        const auto request=view.Hit({700,300});
        const auto expected=!enabled || (!reserved && !eligible) ? Action::None :
            reserved ? Action::CancelContestReservation : Action::ReserveContestHarvest;
        assert(request.action==expected);
        if(expected!=Action::None) assert(request.argument==42 && request.inventoryGeneration==77);
        if(reserved) assert(view.Hit({250,300}).action==Action::None);
    }
    assert(CropSizeText({}) == "--");
    {
        HarvestInventoryViewState state; state.records=128; state.pages=64; state.page=63;
        state.capacity=128; state.rowCount=2; state.inventoryGeneration=99; state.canChangeProtection=true;
        state.contestReservationId=1; state.reservedCrop=farm::CropType::Carrot;
        for(int i=0; i<2; ++i) { state.rows[i].id=127+i; state.rows[i].canReserve=true; }
        View view; BuildHarvestInventoryView(view,state);
        assert(view.Hit({800,450}).action==Action::None);
        assert(view.Hit({200,450}).action==Action::HarvestPage);
        assert(view.Hit({700,300}).argument==127 && view.Hit({700,388}).argument==128);
        bool label=false;
        for(std::size_t i=0;i<view.count;++i) label |= view.items[i].label==Label::ContestCarrot;
        assert(label);
    }
    assert(CropSizeText({1.25f, true}) == "1.25x");
    assert(CropSizeText({std::numeric_limits<float>::quiet_NaN(), true}) == "--");
    for (bool harvested : {false, true}) {
        FarmCropQualityResult quality;
        quality.crop = farm::CropType::TestCrop; quality.basePrice = quality.salePrice = 120;
        quality.harvestSize = {2.f, true};
        View view; BuildQualityView(view, quality, FarmCropQualitySystem::Analyze(quality), 0, harvested);
        bool found = false;
        for (std::size_t i = 0; i < view.count; ++i) {
            if (view.items[i].label == (harvested ? Label::SizeRecorded : Label::SizeForecast)) {
                found = true; assert(view.items[i].value == "2.00x");
                const auto& bottom = QualityRadar::axisLabels[2];
                assert(bottom.y + bottom.height + 8 <= view.items[i].rect.y);
            }
        }
        assert(found);
        for (const auto& label : QualityRadar::axisLabels) {
            assert(label.width == 24 && label.height == 36);
            for (std::size_t i = 0; i < view.count; ++i) {
                const auto& item = view.items[i].rect;
                assert(label.x + label.width <= item.x || item.x + item.width <= label.x ||
                    label.y + label.height <= item.y || item.y + item.height <= label.y);
            }
        }
    }
    for (bool preview : {false,true}) for (bool allowed : {false,true}) for (bool intake : {false,true})
    for (const auto issue : {farm::FarmCanalPathIssue::None, farm::FarmCanalPathIssue::NonStraight,
        farm::FarmCanalPathIssue::BlockedTile}) for (int blocked : {-1, 639}) {
        TerrainViewState state;
        state.preview = preview; state.canConfirm = allowed;
        state.canRaise = state.canLower = state.canCanal = state.canSource = allowed;
        state.canPath = state.canUndo = state.canRedo = state.canCompost = allowed;
        state.canSetIrrigation = allowed; state.irrigationEnabled = intake;
        state.tileValues = "639 / H3 / 100 / 100";
        state.changeCount = preview ? 640 : 0;
        state.pathIssue = issue;
        state.blockedTileIndex = blocked;
        View terrain;
        BuildTerrainView(terrain, state);
        assert(terrain.terrain && !terrain.modal && terrain.count == 18);
        assert(terrain.items[1].selected == (allowed && intake));
        assert(terrain.items[2].selected == (allowed && !intake));
        assert(terrain.Hit({700,40}).action == (!preview && allowed ? Action::SetIrrigation : Action::None));
        assert(terrain.Hit({830,40}).action == (!preview && allowed ? Action::SetIrrigation : Action::None));
        if (!preview && allowed) {
            assert(terrain.Hit({700,40}).argument == 1 && terrain.Hit({830,40}).argument == 0);
        }
        const bool rejected = preview && issue != farm::FarmCanalPathIssue::None;
        const Label expectedStatus = preview && !allowed ? Label::PreviewStale :
            !preview || issue == farm::FarmCanalPathIssue::None ? state.status :
            issue == farm::FarmCanalPathIssue::BlockedTile ? Label::PathBlocked : Label::PathNonStraight;
        assert(terrain.items[0].label == expectedStatus);
        if (expectedStatus == Label::PathBlocked)
            assert(terrain.items[0].value == (blocked < 0 ? "--" : "#639"));
        else assert(terrain.items[0].value.empty());
        assert(terrain.items[14].label == (rejected && allowed ? Label::ConfirmCandidates : Label::Confirm));
        assert(terrain.items[5].value == (preview ? "640" : "0"));
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
        assert(quick.Hit({650,40}).action == Action::Pause);
        assert(quick.items[1].label == (preview ? Label::Play : Label::Pause));
        for (std::size_t i=0; i<quick.count; ++i) {
            const auto& rect = quick.items[i].rect;
            assert(!View::FarmHUDCovers({rect.x,rect.y}));
            assert(!View::FarmHUDCovers({rect.x+rect.width-1,rect.y+rect.height-1}));
        }
    }
    assert(kAsciiY + 128 <= 4096);
    for (bool paused : {false,true}) for (bool canEdit : {false,true})
    for (bool canControlTime : {false,true})
    for (float speed : {1.0f,2.0f,4.0f,3.0f,std::numeric_limits<float>::quiet_NaN()}) {
        View quick;
        BuildPlayQuickView(quick, paused, canEdit, canControlTime, speed);
        assert(quick.count == 5);
        assert(quick.Hit({650,40}).action == (canControlTime ? Action::Pause : Action::None));
        int selected = 0;
        for (std::size_t i = 0; i < quick.count; ++i) {
            const auto& item = quick.items[i];
            assert((item.rect.width-20)/kLabels[static_cast<std::size_t>(item.label)].width >= 20.f/26.f);
            for (const auto& panel : View::kFarmHudPanels) assert(separated(item.rect, panel));
            for (std::size_t j=i+1; j<quick.count; ++j) assert(separated(item.rect,quick.items[j].rect));
            if (i < 2) continue;
            const int expected = i == 2 ? 1 : i == 3 ? 2 : 4;
            assert(item.request.argument == expected);
            const Vector2 center{item.rect.x+item.rect.width/2,item.rect.y+item.rect.height/2};
            const auto hit = quick.Hit(center);
            assert(hit.action == (canControlTime ? Action::Speed : Action::None));
            if (canControlTime) assert(hit.argument == expected);
            assert(quick.Covers(center));
            assert(item.selected == (canControlTime && !paused && speed == expected));
            selected += item.selected ? 1 : 0;
        }
        assert(selected == (canControlTime && !paused && (speed==1 || speed==2 || speed==4) ? 1 : 0));
        assert(quick.Hit({748,40}).action == Action::None);
    }
    assert(farm::kFarmOverviewFrame.bottom * 720 <= View::kWaterGuidancePanel.y - 8);
    for (bool visible : {false,true}) for (bool closed : {false,true}) for (bool editable : {false,true})
    for (bool canSetIntake : {false,true})
    for (auto supply : {farm::FarmWaterStatus::None, farm::FarmWaterStatus::Available,
        farm::FarmWaterStatus::Retained, farm::FarmWaterStatus::Waiting, farm::FarmWaterStatus::Dry})
    for (auto advice : {FarmWaterAdvice::Unknown, FarmWaterAdvice::Till, FarmWaterAdvice::Plant,
        FarmWaterAdvice::Harvest, FarmWaterAdvice::Water, FarmWaterAdvice::CheckSupply,
        FarmWaterAdvice::CloseIntake, FarmWaterAdvice::AvoidWater, FarmWaterAdvice::Monitor}) {
        View status; BuildWaterGuidanceView(status, {visible,closed,supply,advice}, editable, canSetIntake);
        assert(status.count == (visible ? 3u : 0u));
        assert(status.waterGuidance == visible);
        assert(status.Hit({500,460}).action == (visible && editable ? Action::TerrainField : Action::None));
        assert(status.Hit({500,510}).action == Action::None);
        assert(status.Covers({500,490}) == visible);
        const auto intake = status.Hit({940,510});
        assert(intake.action == (visible && editable && canSetIntake ? Action::SetIrrigation : Action::None));
        if (intake.action == Action::SetIrrigation) assert(intake.argument == (closed ? 1 : 0));
        assert(status.Covers({940,510}) == visible);
        assert(status.Hit({882,510}).action == Action::None);
        if (visible) {
            assert(status.items[1].warning == (advice == FarmWaterAdvice::CloseIntake || advice == FarmWaterAdvice::AvoidWater));
            assert(status.items[2].label == (closed ? Label::IntakeOn : Label::IntakeOff));
            assert(status.items[1].rect.x + status.items[1].rect.width <= status.items[2].rect.x);
        }
        if (visible && closed) assert(status.items[0].label == Label::IntakeClosed);
        for (std::size_t i=0; i<status.count; ++i) {
            const auto& item = status.items[i];
            assert(!View::FarmHUDCovers({item.rect.x,item.rect.y}));
            assert(!View::FarmHUDCovers({item.rect.x+item.rect.width-1,item.rect.y+item.rect.height-1}));
            assert((item.rect.width-20)/kLabels[static_cast<std::size_t>(item.label)].width >= 20.0f/26.0f);
        }
        if (visible) assert(status.items[0].rect.y+status.items[0].rect.height <= status.items[1].rect.y);
    }
    for (const Label label : {Label::RaiseBrush, Label::LowerBrush}) {
        TerrainViewState state;
        state.preview = state.canConfirm = true; state.status = label;
        View heightView; BuildTerrainView(heightView, state);
        assert(heightView.items[0].label == label && heightView.count == 18);
        assert((heightView.items[0].rect.width - 20) / kLabels[static_cast<std::size_t>(label)].width >= 20.0f/26.0f);
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
    for (const auto frame : {farm::kFarmOverviewFrame, farm::kObservationOverviewFrame, farm::kTerrainOverviewFrame}) {
        for (float aspect : {4.f/3, 16.f/9, 21.f/9}) for (float x : {-8.f, 0.f, 8.f})
        for (float z : {-4.f, 8.f, 18.f}) for (float height : {0.f, 3.f}) {
            const Vector3 farmMin{-3,0,5}, farmMax{3,2,11};
            const Vector3 playerCenter{x,height+.9f,z}, playerHalf{.45f,.9f,.45f};
            const float py = 1/std::tan(.45f/2), px = py/aspect;
            farm::OverviewPose pose;
            assert(farm::FitFarmFollowCamera(farmMin, farmMax, playerCenter, playerHalf,
                px, py, .1f, 1000, frame, pose));
            const std::array minima{farmMin, Vector3{x-.70f,height-.25f,z-.70f}};
            const std::array maxima{farmMax, Vector3{x+.70f,height+2.05f,z+.70f}};
            for (std::size_t box=0; box<minima.size(); ++box) for (int corner=0; corner<8; ++corner) {
                const auto& low=minima[box]; const auto& high=maxima[box];
                const float dx=((corner&1)?high.x:low.x)-pose.position.x;
                const float dy=((corner&2)?high.y:low.y)-pose.position.y;
                const float dz=((corner&4)?high.z:low.z)-pose.position.z;
                const float cameraY=std::cos(pose.rotation.x)*dy+std::sin(pose.rotation.x)*dz;
                const float cameraZ=-std::sin(pose.rotation.x)*dy+std::cos(pose.rotation.x)*dz;
                const float screenX=(px*dx/cameraZ+1)*.5f, screenY=(1-py*cameraY/cameraZ)*.5f;
                assert(cameraZ>.1f && cameraZ<1000);
                assert(screenX>=frame.left-.0001f && screenX<=frame.right+.0001f);
                assert(screenY>=frame.top-.0001f && screenY<=frame.bottom+.0001f);
            }
        }
    }
    farm::OverviewPose unchanged{{123,456,789}, {0,0,0}};
    for (float invalid : {0.f,-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
        assert(!farm::FitFarmFollowCamera({0,0,0},{1,1,1},{0,0,0},{invalid,1,1},2,4,.1f,100,
            farm::kFarmOverviewFrame,unchanged));
    assert(!farm::FitFarmFollowCamera({2,0,0},{1,1,1},{0,0,0},{1,1,1},2,4,.1f,100,farm::kFarmOverviewFrame,unchanged));
    assert(!farm::FitFarmFollowCamera({0,0,0},{1,1,1},{std::numeric_limits<float>::quiet_NaN(),0,0},
        {1,1,1},2,4,.1f,100,farm::kFarmOverviewFrame,unchanged));
    assert(!farm::FitFarmFollowCamera({0,0,0},{1,1,1},{0,0,0},{1,1,1},2,4,.1f,1,farm::kFarmOverviewFrame,unchanged));
    std::cout << "PASS: follow camera farm/player safe bounds, 162 poses, invalid input retains pose\n";
    for (float invalid : {0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        assert(!farm::FitOverviewCamera({0,0,0}, {1,1,1}, invalid, 4, 0.1f, 100, farm::kFarmOverviewFrame, unchanged));
    assert(!farm::FitOverviewCamera({2,0,0}, {1,1,1}, 2, 4, 0.1f, 100, farm::kFarmOverviewFrame, unchanged));
    assert(!farm::FitOverviewCamera({0,0,0}, {1,1,1}, 2, 4, 0.1f, 1, farm::kFarmOverviewFrame, unchanged));
    assert(!farm::FitOverviewCamera({0,0,0}, {1,1,1}, 2, 4, 0.1f, 100, {0,0,0,1}, unchanged));
    for (float pitch : {0.f, 1.4f, std::numeric_limits<float>::quiet_NaN()})
        assert(!farm::FitOverviewCamera({0,0,0}, {1,1,1}, 2, 4, .1f, 100,
            farm::kFarmOverviewFrame, unchanged, pitch));
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
        View ended;
        BuildFieldActionView(ended, evaluation, false, true, FarmHUDNextAction::SelectTile, true);
        assert(ended.items[1].label == Label::SeasonEnded && !ended.items[2].enabled);
        View stopped;
        BuildPlayQuickView(stopped, false, false, false);
        assert(stopped.items[1].label == Label::Paused && stopped.Hit({650,40}).action == Action::None);
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
            if (blocked & 2) assert(hit.action == Action::None);
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
                const auto expected = (blocked & 2) != 0 ? Action::None : status == FarmToolActionStatus::NoSeed
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
        assert(uv.x >= 0 && uv.x + uv.width <= kAtlasWidth && kAtlasWidth <= 4096);
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
