#include "farm/ui/FarmHarvestDisplayView.h"
#include "farm/system/FarmDocumentSystem.h"
#include "farm/core/FarmGrid.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
void Check(bool ok, const char* text) { if (!ok) { std::cerr << text << '\n'; std::exit(1); } }
void Layout(const FarmHarvestDisplaySystem::Snapshot& state) {
    farmui::View view;
    farmui::BuildHarvestDisplayView(view, state, true);
    Check(view.modal && view.harvestDisplay && view.count < view.kCapacity, "modal capacity");
    Check(view.Covers({0,400}), "no farm click through");
    for (std::size_t i=0; i<view.count; ++i) {
        const auto& a=view.items[i];
        if (a.label==farmui::Label::DisplayPoints) Check(a.darkInk,"counter metrics use contrasting ink");
        if (!a.value.empty() && a.value.front()=='#') {
            Check(a.darkInk,"all record signs use contrasting ink");
        }
        Check(a.rect.x>=0 && a.rect.y>=0 && a.rect.x+a.rect.width<=1280 && a.rect.y+a.rect.height<=720, "screen bounds");
        if (a.request.action!=farmui::Action::HarvestDisplaySelect) {
            const auto width=farmui::kLabels[static_cast<std::size_t>(a.label)].width;
            const auto available=a.value.empty()?a.rect.width-20:a.valueOffset-30;
            if (available/width <20.f/26.f) { std::cerr << "label " << static_cast<int>(a.label) << '\n'; Check(false,"legible labels"); }
            if (!a.value.empty()) Check(a.valueOffset+14*(a.value.size()-1)+24<=a.rect.width,"numeric fit");
        }
        for (std::size_t j=i+1;j<view.count;++j) {
            const auto& b=view.items[j];
            if (a.request.action==farmui::Action::HarvestDisplaySelect && b.request.action==farmui::Action::None &&
                a.rect.Contains({b.rect.x,b.rect.y}) && a.rect.Contains({b.rect.x+b.rect.width-1,b.rect.y+b.rect.height-1})) continue;
            Check(a.rect.x+a.rect.width<=b.rect.x || b.rect.x+b.rect.width<=a.rect.x ||
                a.rect.y+a.rect.height<=b.rect.y || b.rect.y+b.rect.height<=a.rect.y,"nonoverlap");
        }
    }
    for (int i=0;i<state.count;++i) {
        const auto& target=farmui::SeedShopLayout::products[i];
        const auto hit=view.Hit({target.x+100,200});
        Check(hit.action==farmui::Action::HarvestDisplaySelect && hit.argument==state.records[i].id && hit.inventoryGeneration==state.generation,"record identity hit");
        for (float y : {470.f,518.f}) {
            const auto signHit=view.Hit({target.x+160,y});
            Check(signHit.action==farmui::Action::HarvestDisplaySelect && signHit.argument==state.records[i].id &&
                signHit.inventoryGeneration==state.generation,"sign selects matching record and generation");
        }
    }
    for (int i=state.count;i<FarmHarvestDisplaySystem::kSlots;++i)
        Check(view.Hit({farmui::SeedShopLayout::products[i].x+100,518}).action==farmui::Action::None,"empty sign cannot select a record");
}
FarmCropQualityResult Quality(farm::CropType crop) {
    farm::FarmTile tile;
    tile.state=farm::FarmTileState::Planted; tile.crop=crop; tile.growth=1;
    tile.careHistory.goodSeconds=10; tile.careHistory.efficiencySeconds=10;
    tile.careHistory.nutrientGrowth=1; tile.careHistory.nutrientSupply=0.8f;
    FarmCropQualitySystem system; system.Initialize(); return system.Evaluate(tile);
}
}
int main(int argc, char** argv) {
    FarmEconomySystem economy; economy.Initialize();
    FarmHarvestDisplaySystem display;
    auto state=display.Observe(economy,1);
    Check(state.count==0 && state.selectedSlot==-1 && state.pages==1,"empty"); Layout(state);
    Check(economy.AddHarvest(farm::CropType::Carrot,2),"legacy inventory");
    state=display.Observe(economy,1); Check(state.unknownCount==2 && state.count==0,"unknown records not invented"); Layout(state);
    const auto turnip=Quality(farm::CropType::TestCrop), carrot=Quality(farm::CropType::Carrot);
    Check(economy.AddHarvest(turnip,1,1) && economy.AddHarvest(turnip,1,2) && economy.AddHarvest(carrot,1,3),"three distinct records");
    const int money=economy.GetMoney(), total=economy.GetTotalCropCount();
    state=display.Observe(economy,3); Check(state.pages==2 && state.count==2 && state.selectedSlot==0,"first page"); Layout(state);
    const int second=state.records[1].id;
    Check(!display.Select(economy,-1,state.generation) && !display.Select(economy,second,state.generation+1),"bad stale selection");
    Check(display.Select(economy,second,state.generation),"select second same crop");
    state=display.Observe(economy,3);
    Check(state.selectedSlot==1 && state.records[1].id==second && state.records[1].harvestedDay==2,"correct record projection");
    Check(economy.GetMoney()==money && economy.GetTotalCropCount()==total && economy.GetContestReservationId()==0,"browsing does not mutate");
    const auto stale=state.generation;
    Check(economy.SetHarvestProtection(second,true,stale) && economy.SetContestReservation(second,true,stale),"protect and reserve existing command");
    Check(economy.RestoreSnapshot(economy.CaptureSnapshot()),"reload invalidates generation");
    Check(!display.Select(economy,second,stale),"generation changes invalidate old clicks");
    Check(!economy.SetContestReservation(state.records[0].id,true,stale),"stale mutation rejected");
    state=display.Observe(economy,3); Check(state.reservationId==second && state.selectedSlot==1,"reservation stays identifiable"); Layout(state);
    Check(state.judge.recordId==second && state.entry.harvestedDay==2,"judge and period match reserved record");
    display.MovePage(economy,1); state=display.Observe(economy,3);
    Check(state.page==1 && state.count==1 && state.records[0].quality.crop==farm::CropType::Carrot,"last partial page"); Layout(state);
    Check(!display.Select(economy,second,state.generation),"off-page selection rejected");
    display.MovePage(economy,999); Check(display.Observe(economy,3).page==1,"invalid step");
    display.MovePage(economy,1); Check(display.Observe(economy,3).page==1,"upper bound");
    Check(economy.SetHarvestProtection(state.records[0].id,true,state.generation),"protect for period preview");
    state=display.Observe(economy,20); Check(state.entry.issue==FarmContestEntryIssue::OutsidePeriod,"period rejection"); Layout(state);
    Check(economy.SetHarvestProtection(state.records[0].id,false,state.generation),"unprotect nonreserved fixture");
    state=display.Observe(economy,31); Check(state.entry.issue==FarmContestEntryIssue::SeasonEnded,"ended"); Layout(state);
    Check(economy.SellAll().Succeeded(),"sell unprotected records");
    state=display.Observe(economy,3); Check(state.page==0 && state.count==1 && state.records[0].id==second,"shrinking page clamped to remaining reserved record"); Layout(state);
    Check(!FarmContestSubmissionSystem::Submit(economy,10,second,state.generation+1) &&
        !FarmContestSubmissionSystem::Submit(economy,10,second+1,state.generation),"stale and wrong submission rejected");
    Check(FarmContestSubmissionSystem::Submit(economy,10,second,state.generation),"selected reserved record submits through existing system");
    Check(!FarmContestSubmissionSystem::Submit(economy,10,second,state.generation),"duplicate submission rejected");
    state=display.Observe(economy,10); Check(state.count==0 && state.reservationId==0 && economy.GetContestResults()[0].harvest.id==second,"submitted record consumed exactly once"); Layout(state);
    economy.Initialize(); display.Reset();
    Check(economy.AddHarvest(carrot,1,0),"unknown harvest date");
    Check(economy.SetHarvestProtection(economy.GetHarvestRecord(0)->id,true,economy.GetInventoryGeneration()),"protect unknown-day fixture");
    state=display.Observe(economy,3); Check(state.entry.issue==FarmContestEntryIssue::UnknownHarvestDay,"unknown day"); Layout(state);
    farmui::View view; farmui::BuildHarvestDisplayView(view,state,true);
    Check(view.Hit({1000,400}).action==farmui::Action::None,"ineligible cannot reserve");
    state.records[0].id=(std::numeric_limits<int>::max)();
    state.records[0].quantity=(std::numeric_limits<int>::max)();
    state.records[0].harvestedDay=(std::numeric_limits<int>::max)(); Layout(state);
    std::cout<<"Harvest display: identity, no mutation, stale/page/period/legacy guards and layout PASS\n";
    if (argc == 2 && std::string(argv[1]) == "--fixture") {
        farm::FarmGrid grid; Check(grid.Initialize(5,4),"fixture grid");
        FarmEconomySystem fixtureEconomy; fixtureEconomy.Initialize();
        FarmCropSelectionSystem crops; crops.Initialize();
        FarmDateSystem date; date.Initialize();
        FarmProgressionSystem progression; progression.Initialize({},FarmProgressionMode::ContestSeason);
        FarmDocumentSystem documents;
        const std::string directory="generated/codex_checks/harvest_display_fixture_"+
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        Check(documents.Initialize(directory,grid,fixtureEconomy,crops,&date,&progression),"fixture document");
        Check(date.RestoreSnapshot({10,0,1}),"fixture contest day");
        Check(fixtureEconomy.AddHarvest(turnip,1,8) && fixtureEconomy.AddHarvest(carrot,1,9) &&
            fixtureEconomy.AddHarvest(turnip,1,10),"fixture harvests");
        Check(documents.SaveAs("Harvest display QA",grid,fixtureEconomy,crops),"fixture save");
        std::cout<<"QA fixture (synthetic, not weekly evidence): "<<directory<<'\n';
    }
}
