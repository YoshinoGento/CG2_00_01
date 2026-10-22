#include "farm_test_edit.h"
#include "farm/system/FarmStartPresentationSystem.h"
#include "farm/system/FarmPlayFlow.h"
#include "farm/system/FarmDateSystem.h"
#include <cassert>
#include <cstdio>
#include <limits>

using System = FarmStartPresentationSystem;

static void TestArrivalEligibility(farm::OverviewPose final) {
    farm::FarmGrid field;
    assert(!System::SupportsArrival(field));
    for (const auto size : {std::array{4, 5}, std::array{6, 4}, std::array{5, 3}}) {
        assert(field.Initialize(size[0], size[1]));
        assert(!System::SupportsArrival(field));
    }
    assert(field.Initialize(System::kArrivalGridWidth, System::kArrivalGridHeight));
    assert(System::SupportsArrival(field));
    const auto generation = field.GetGeneration();
    assert(field.SetSelectedIndex(19));
    assert(System::SupportsArrival(field) && field.GetSelectedIndex() == 19);
    assert(field.GetGeneration() == generation);

    // Every cell, including the last, can disqualify the authored empty-field opening.
    std::array<farm::FarmTile, 10> edited{};
    edited[0].state = farm::FarmTileState::Tilled;
    edited[1].state = farm::FarmTileState::Planted;
    edited[2].crop = farm::CropType::Carrot;
    edited[3].feature = farm::FarmTileFeature::WaterSource;
    edited[4].feature = farm::FarmTileFeature::Canal;
    edited[5].heightLevel = 1;
    edited[6].moisture = .5f;
    edited[7].growth = .5f;
    edited[8].moisture = std::numeric_limits<float>::quiet_NaN();
    edited[9].growth = std::numeric_limits<float>::infinity();
    for (int i = 0; i < field.GetTileCount(); ++i) {
        for (const auto& changed : edited) {
            assert(field.SetTile(i, {}));
            const bool valid = farm::FarmGrid::IsValidTile(changed);
            assert(field.SetTile(i, changed) == valid);
            assert(System::SupportsArrival(field) == !valid);
        }
        assert(field.SetTile(i, {}));
        assert(System::SupportsArrival(field));
    }

    FarmDateSystem clock;
    // The reported save is day1 at1.5166659s, with all20 cells still empty.
    for (const auto savedClock : {FarmDateSystem::Snapshot{1, 1.5166659f, 1},
        FarmDateSystem::Snapshot{2, 0, 1}, FarmDateSystem::Snapshot{30, 12, 4}}) {
        assert(clock.RestoreSnapshot(savedClock));
        farm::FarmGrid::Snapshot savedField;
        field.CaptureSnapshot(savedField);
        farm::FarmGrid loaded;
        assert(loaded.Initialize(savedField.width, savedField.height));
        assert(loaded.RestoreSnapshot(savedField));
        System presentation;
        FarmPlayFlow flow;
        // Reuse the timeline on re-entry/load/restart, with no playback-history veto.
        for (int entry = 0; entry < 3; ++entry) {
            flow.Reset();
            assert(System::SupportsArrival(loaded));
            assert(presentation.BeginArrival(final));
            flow.BeginStart();
            float seconds = 0;
            while (presentation.Active() && seconds < 16) {
                assert(flow.BlocksSimulation());
                presentation.Update(1.0f / 60, false, false);
                seconds += 1.0f / 60;
                assert(presentation.GetFrame().arrival);
            }
            assert(seconds >= farm::arrival::kDuration && seconds < farm::arrival::kDuration + .05f);
            assert(presentation.ConsumeCompleted() && !presentation.ConsumeCompleted());
            flow.FinishStart();
            flow.BeginStart(); // Ordinary menu/control updates cannot re-enter Starting from Playing.
            assert(flow.GetPhase() == FarmPlayFlow::Phase::Playing && !flow.BlocksSimulation());
            assert(clock.GetDay() == savedClock.day);
            assert(clock.GetElapsedSecondsInDay() == savedClock.elapsedSecondsInDay);
            assert(loaded.GetSelectedIndex() == 19 && System::SupportsArrival(loaded));
        }
    }
}

int main() {
    const farm::OverviewPose initial{{0, 15, -10}, {.7f, 0, 0}};
    const farm::OverviewPose final{{2, 18, -12}, {.7f, 0, 0}};
    TestArrivalEligibility(final);
    std::array<System::Cell, 9> cells{};
    for (std::size_t i = 0; i < cells.size(); ++i) {
        cells[i].pose = {{static_cast<float>(i%3), 10, static_cast<float>(i/3)}, {.86f,0,0}};
    }
    cells[0].source = true;
    for (int i : {1,2,5,8}) cells[static_cast<std::size_t>(i)].canal = true;
    cells[1].upstream=0; cells[2].upstream=1; cells[5].upstream=2; cells[8].upstream=5;
    System system;
    assert(system.Begin(cells,3,initial,final) && system.RouteCount()==5);
    const auto first = system.GetFrame();
    assert(first.pose.position.y==initial.position.y && !first.canSkip);
    system.Update(0.1f,true,true);
    assert(!system.GetFrame().canSkip && system.Active());
    system.Update(0.0f,false,false);
    assert(system.GetFrame().canSkip);
    system.Update(-1,false,false);
    system.Update(std::numeric_limits<float>::quiet_NaN(),false,false);
    system.Update(std::numeric_limits<float>::infinity(),false,false);
    assert(std::isfinite(system.GetFrame().pose.position.y));
    bool sawSource=false,sawCanal=false,sawGoal=false;
    float oldY=system.GetFrame().pose.position.y;
    for (int i=0;i<430;++i) {
        system.Update(1.0f/60,false,false);
        const auto frame=system.GetFrame();
        sawSource |= frame.subject==System::Subject::Source;
        sawCanal |= frame.subject==System::Subject::Canal;
        sawGoal |= frame.showGoal;
        assert(frame.bars>=0 && frame.bars<=1);
        assert(frame.pose.position.y>=10 && frame.pose.position.y<=18);
        assert(std::abs(frame.pose.position.y-oldY)<0.25f);
        oldY=frame.pose.position.y;
    }
    assert(sawSource && sawCanal && sawGoal && !system.Active());
    assert(system.ConsumeCompleted() && !system.ConsumeCompleted());
    assert(system.GetFrame().pose.position.x==final.position.x);

    assert(system.Begin(cells,3,initial,final));
    system.Update(.1f,false,false);
    system.Update(.1f,true,true);
    for(int i=0;i<100;++i) system.Update(.1f,true,false);
    assert(system.Active() && !system.ConsumeCompleted());
    assert(system.GetFrame().pose.position.z==final.position.z);
    system.Update(.01f,false,false);
    assert(!system.Active() && system.ConsumeCompleted());
    assert(system.Begin({},0,initial,final) && system.RouteCount()==0);
    assert(system.GetFrame().subject==System::Subject::Farm);
    assert(system.GetFrame().showGoal);
    for(int i=0;i<31;++i) system.Update(.1f,false,false);
    assert(system.ConsumeCompleted() && !system.Active());
    cells[0].source=false;
    cells[0].canal=true; cells[0].upstream=1;
    assert(system.Begin(cells,3,initial,final) && system.RouteCount()==0);
    cells[0].source=true; cells[8].upstream=0; // Non-adjacent links are rejected.
    assert(system.Begin(cells,3,initial,final) && system.RouteCount()==4);
    cells[5].upstream=99999;
    assert(system.Begin(cells,3,initial,final) && system.RouteCount()==3);
    cells[2].pose.position.x=std::numeric_limits<float>::quiet_NaN();
    assert(system.Begin(cells,3,initial,final) && system.RouteCount()==2);
    assert(system.Begin(cells,4,initial,final) && system.RouteCount()==0);
    std::array<System::Cell, System::kMaximumCells+1> oversized{};
    assert(system.Begin(oversized,1,initial,final) && system.RouteCount()==0);
    auto bad=initial; bad.position.x=std::numeric_limits<float>::infinity();
    assert(!system.Begin(cells,3,bad,final) && !system.Active());
    assert(!system.BeginArrival(bad));
    assert(system.BeginArrival(final));
    assert(system.GetFrame().arrival && system.GetFrame().cover==1);
    // Macro camera must contain both the rooted base and the hanging tip inside the bars.
    for(float time:{0.0f,1.3f,2.6f}) {
        const auto f=farm::arrival::Sample(time,final);
        for(const auto p:{farm::arrival::kAnchor,farm::arrival::LeafTip(time)}) {
            const Vector3 d{p.x-f.pose.position.x,p.y-f.pose.position.y,p.z-f.pose.position.z};
            const float yaw=f.pose.rotation.y, pitch=f.pose.rotation.x;
            const float z=d.x*std::sin(yaw)+d.z*std::cos(yaw);
            const float vy=d.y*std::cos(pitch)+z*std::sin(pitch);
            const float vz=-d.y*std::sin(pitch)+z*std::cos(pitch);
            assert(vz>0 && std::abs(vy/(vz*std::tan(.45f*.5f)))<.75f);
        }
    }
    float previousDrop=farm::arrival::Sample(farm::arrival::kRelease,final).drop.y;
    for(int i=0;i<=100;++i) {
        const float time=farm::arrival::kRelease+(farm::arrival::kImpact-farm::arrival::kRelease)*static_cast<float>(i)/100;
        const auto frame=farm::arrival::Sample(time,final);
        assert(frame.drop.y<=previousDrop+.00001f && frame.drop.y>=farm::arrival::kAnchor.y);
        previousDrop=frame.drop.y;
    }
    bool sawDrop=false,sawWet=false,sawArrivalDay=false;
    for(int i=0;i<900;++i) {
        system.Update(1.0f/60,false,false);
        const auto frame=system.GetFrame();
        const auto arrival=farm::arrival::Sample(frame.arrivalTime,final);
        sawDrop|=arrival.dropScale>0; sawWet|=arrival.wetness>.9f; sawArrivalDay|=frame.showGoal;
        assert(std::isfinite(frame.pose.position.x) && frame.pose.position.y>0);
        assert(frame.cover>=0 && frame.cover<=1 && frame.bars>=0 && frame.bars<=1);
    }
    assert(sawDrop && sawWet && sawArrivalDay && !system.Active() && system.ConsumeCompleted());
    assert(system.GetFrame().pose.position.x==final.position.x && system.GetFrame().bars==0);
    for(const float skipAt:{0.0f,2.8f,6.0f,10.0f,13.5f}) {
        assert(system.BeginArrival(final));
        for(float t=0;t<skipAt;t+=.05f) system.Update(.05f,false,false);
        system.Update(0,false,false);
        system.Update(.01f,true,true);
        for(int i=0;i<10;++i) system.Update(.1f,true,false);
        assert(system.Active() && system.GetFrame().pose.position.x==final.position.x);
        system.Update(.01f,false,false);
        assert(system.ConsumeCompleted() && !system.ConsumeCompleted());
    }
    FarmPlayFlow flow;
    assert(flow.BlocksSimulation()); flow.BeginStart();
    assert(flow.GetPhase()==FarmPlayFlow::Phase::Starting && flow.BlocksSimulation());
    flow.Continue(); assert(flow.GetPhase()==FarmPlayFlow::Phase::Starting);
    flow.FinishStart(); assert(flow.GetPhase()==FarmPlayFlow::Phase::Playing && !flow.BlocksSimulation());
    assert(!flow.RequestsTitle());
    flow.RequestTitle(); assert(flow.RequestsTitle() && flow.BlocksSimulation());
    flow.Reset(); assert(!flow.RequestsTitle()); flow.Continue();
    flow.Observe(true); flow.Continue(); assert(flow.GetPhase()==FarmPlayFlow::Phase::Review);
    flow.Reset(); assert(flow.GetPhase()==FarmPlayFlow::Phase::Briefing);
    std::puts("PASS: empty-field eligibility after elapsed time/day/load/re-entry, edited/invalid field rejection, connected route, cycles/OOB/nonfinite/oversize fallbacks, camera continuity, held start/skip, exact completion, play-flow gate");
}
