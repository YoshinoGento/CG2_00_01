#include "title/TitleAudioSettingsSystem.h"
#include "io/JsonFile.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>
namespace {
void Check(bool result, std::source_location at = std::source_location::current()) {
    if (!result) throw std::runtime_error("settings assertion at line "+std::to_string(at.line()));
}
title::AudioSettingsInput Click(float x, float y, bool held = true) {
    title::AudioSettingsInput input;
    input.pointer = {x,y}; input.pointerValid = true; input.pressed = true; input.held = held;
    return input;
}
}
int main() {
    const std::filesystem::path dir = "generated/codex_checks/title_audio_settings_tests";
    std::filesystem::create_directories(dir);
    const auto path = (dir/"settings.json").string();
    std::filesystem::remove(path);
    title::TitleAudioSettingsSystem system;
    system.Initialize(path);
    Check(system.GetFrame().volume == std::array<int,2>{40,40});
    const auto mix = system.Apply(title::MakeAudioMix(0,0));
    Check(std::abs(mix.ambience[0]-.18f)<.00001f && std::abs(mix.water-.12f)<.00001f);
    for (int i=0; i<4000; ++i) {
        const auto scaled = system.Apply(title::MakeAudioMix(i*.01f,0));
        float total = 0; for (auto gain : scaled.ambience) total += gain;
        Check(total<=title::kAmbienceGain*.4f+.00001f && scaled.water<=title::kWaterGain*.4f+.00001f);
    }
    Check(system.Update(Click(640,650)));
    Check(!system.Update(Click(1100,60)) && system.GetFrame().open);
    Check(!system.Update(Click(640,650))); // Start rectangle is modal-blocked.
    Check(!system.Update(Click(title::kAudioSliderX,310)) && system.GetFrame().volume[0]==0);
    auto drag = Click(2000,310); drag.pressed = false;
    Check(!system.Update(drag) && system.GetFrame().volume[0]==100);
    Check(!JsonFile::Exists(path)); // Drag frames do not write settings.
    system.Update({});
    drag.pointer.x = 600; system.Update(drag);
    Check(system.GetFrame().volume[0]==100); // Release ends capture.
    auto nan = Click(std::numeric_limits<float>::quiet_NaN(),310); system.Update(nan);
    Check(system.GetFrame().volume[0]==100);
    system.Update(Click(title::kAudioSliderX+title::kAudioSliderWidth*.25f,398,false));
    Check(system.GetFrame().volume[1]==25);
    title::AudioSettingsInput right; right.right = true;
    system.Update(right); Check(system.GetFrame().volume[1]==30);
    title::AudioSettingsInput up; up.up = true;
    system.Update(up); Check(system.GetFrame().selected==0);
    system.Update(Click(816,500));
    Check(!system.GetFrame().open && JsonFile::Exists(path));
    title::TitleAudioSettingsSystem restored; restored.Initialize(path);
    Check(restored.GetFrame().volume == std::array<int,2>{100,30});
    Check(!restored.Update(Click(1100,60)));
    Check(!restored.Update(title::AudioSettingsInput{.confirm=true}) && !restored.GetFrame().open);
    Check(restored.Update(title::AudioSettingsInput{.confirm=true}));
    Check(!restored.Update(title::AudioSettingsInput{.toggle=true,.confirm=true}));
    restored.Update(Click(400,500));
    Check(restored.GetFrame().volume==std::array<int,2>{40,40});
    Check(restored.Save());
    restored.Update(Click(title::kAudioSliderX,310));
    restored.Update(Click(title::kAudioSliderX,398));
    const auto muted = restored.Apply(title::MakeAudioMix(9,0));
    for (auto gain : muted.ambience) Check(gain==0);
    Check(muted.water==0);
    Check(JsonFile::Save(path,{{"version",1},{"environmentPercent",-300},{"waterPercent",1e100}}));
    restored.Initialize(path); Check(restored.GetFrame().volume==std::array<int,2>{0,100});
    Check(JsonFile::Save(path,{{"version",1},{"environmentPercent","loud"},{"waterPercent",nullptr}}));
    restored.Initialize(path); Check(restored.GetFrame().volume==std::array<int,2>{40,40});
    Check(JsonFile::Save(path,{{"version",2},{"environmentPercent",100}}));
    restored.Initialize(path); Check(restored.GetFrame().volume==std::array<int,2>{40,40});
    { std::ofstream file(path); file << "broken JSON"; }
    restored.Initialize(path); Check(restored.GetFrame().volume==std::array<int,2>{40,40});
    const auto blocker = dir/"not_a_directory";
    { std::ofstream file(blocker); file << "fixture"; }
    restored.Initialize((blocker/"settings.json").string());
    restored.Update(title::AudioSettingsInput{.toggle=true}); restored.Update(right);
    Check(!restored.Save() && restored.GetFrame().saveFailed);
    Check(restored.GetFrame().volume[0]==45);
    std::cout << "PASS title volume: defaults/mix/modal/drag/release/keyboard/persistence/corrupt/failure\n";
}
