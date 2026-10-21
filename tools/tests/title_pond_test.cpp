#include "title/TitlePondSystem.h"
#include "io/JsonFile.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>

namespace {
void Check(bool result, std::source_location at = std::source_location::current()) {
    if (!result) {
        std::cerr << "pond assertion at line " << at.line() << std::endl;
        throw std::runtime_error("pond assertion at line "+std::to_string(at.line()));
    }
}
}
int main() {
    try {
    const std::string path = "project/Resources/title/pond_layout.json";
    title::TitlePondSystem system;
    Check(system.ProjectPondPoint({}).x < 0);
    Check(system.Initialize(path));
    for (int y=-9; y<=9; ++y) for (int x=-9; x<=9; ++x) {
        const Vector2 point{x*.05f,y*.05f};
        const auto screen = system.ProjectPondPoint(point);
        const auto recovered = system.MapToPond(screen);
        Check(recovered.has_value());
        if (std::abs(recovered->x-point.x)>=.001f || std::abs(recovered->y-point.y)>=.001f)
            std::cerr << "point " << point.x << "," << point.y << " uv " << screen.x << "," << screen.y
                << " recovered " << recovered->x << "," << recovered->y << std::endl;
        Check(std::abs(recovered->x-point.x)<.001f && std::abs(recovered->y-point.y)<.001f);
    }
    Check(!system.MapToPond({.5f,0}));
    Check(!system.MapToPond({std::numeric_limits<float>::quiet_NaN(),.5f}));
    Check(system.ProjectPondPoint({std::numeric_limits<float>::quiet_NaN(),0}).x < 0);
    Check(!system.RequestStart(Vector2{-1,0}));
    Check(!system.RequestStart(Vector2{0,std::numeric_limits<float>::infinity()}));
    int clicks = 0;
    for (int y=0; y<=12; ++y) for (int x=0; x<=16; ++x) {
        Check(system.Initialize(path));
        const Vector2 pointer{x*80.0f,y*60.0f};
        if (!system.RequestStart(pointer)) {
            Check(!system.GetStartPath().enabled);
            // No visible water below this pixel: ignore, do not bend the flight sideways.
            for(int row=y*60; row<720; ++row) {
                const auto p=system.MapToPond({pointer.x/1280,row/720.0f});
                Check(x==0 || x==16 || !p || std::hypot(p->x,p->y)>=.994f);
            }
            continue;
        }
        const auto& route = system.GetStartPath();
        Check(route.enabled);
        const Vector2 hit{route.impact.x,route.impact.y};
        Check(hit.x>0 && hit.x<1 && hit.y>0 && hit.y<1);
        const auto water = system.MapToPond(hit);
        Check(water && std::hypot(water->x,water->y)<.995f);
        Check(route.start.x==route.impact.x && route.start.y<route.impact.y);
        Check(std::abs(route.impact.x-pointer.x/1280)<.00001f);
        const auto clicked=system.MapToPond({pointer.x/1280,pointer.y/720});
        if(clicked && std::hypot(clicked->x,clicked->y)<.994f)
            Check(std::abs(route.impact.y-pointer.y/720)<.00001f);
        Check(!system.RequestStart(Vector2{640,360}));
        scene::SceneTransitionSystem transition;
        Check(transition.Begin({},scene::TransitionTarget::SceneReplacement,
            scene::TransitionSoundTheme::Water,scene::TransitionStyle::Ripple,route));
        Check(transition.GetFrame().center.x == route.impact.x && transition.GetFrame().pond.enabled);
        transition.Update(.25f,false);
        Check(transition.GetFrame().progress == 0 && transition.ConsumeSoundCues() == 0);
        transition.Update(.10f,false);
        Check(transition.GetFrame().progress > 0);
        Check(transition.ConsumeSoundCues() == scene::SoundBit(scene::TransitionSoundCue::WaterDrop));
        float previous = transition.GetFrame().progress;
        while (transition.GetFrame().progress < 1) {
            transition.Update(.016f,false);
            Check(transition.GetFrame().progress >= previous);
            previous = transition.GetFrame().progress;
        }
        Check(!transition.CanSwap());
        transition.NotifyCoverPresented(); Check(transition.BeginReveal());
        Check(transition.GetFrame().pond.enabled && !transition.GetFrame().drops[0].active);
        while (transition.GetFrame().phase == scene::TransitionPhase::Revealing) transition.Update(.016f,false);
        Check(transition.IsActive()); transition.Update(0,true);
        Check(!transition.GetFrame().pond.enabled);
        Check(transition.Begin() && !transition.GetFrame().pond.enabled);
        ++clicks;
    }
    Check(system.Initialize(path) && system.RequestStart());
    int exactHits=0;
    for(int y=1;y<72;++y) for(int x=1;x<128;++x) {
        Check(system.Initialize(path));
        const Vector2 uv{x/128.0f,y/72.0f};
        const auto p=system.MapToPond(uv);
        if(!p || std::hypot(p->x,p->y)>=.994f) continue;
        Check(system.RequestStart(Vector2{uv.x*1280,uv.y*720}));
        const auto hit=system.GetStartPath().impact;
        Check(std::abs(hit.x-uv.x)<.00001f && std::abs(hit.y-uv.y)<.00001f);
        ++exactHits;
    }
    Check(exactHits>100);
    Check(system.Initialize(path));
    title::Frame frame;
    frame.nightAmount = 1;
    system.Update(.25f,frame);
    Check(system.GetFrame().nightAmount == 1 && system.GetFrame().seconds == .25f);
    for (float invalid : {-1.0f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})
        system.Update(invalid,frame);
    Check(system.GetFrame().seconds == .25f);
    nlohmann::json config;
    Check(JsonFile::Load(path,config));
    const std::string fixture = "generated/codex_checks/invalid_pond.json";
    const auto validConfig = config;
    for (int invalid=0; invalid<5; ++invalid) {
        auto bad = validConfig;
        if (invalid==0) bad["outfall"][1][1] = 1;
        if (invalid==1) bad["outfall"][2][1] = 0;
        if (invalid==2) bad["outfall"][2][2] = 10;
        if (invalid==3) bad["outfallHalfWidth"] = 2;
        if (invalid==4) bad["outfall"] = {{0,0,0}};
        Check(JsonFile::Save(fixture,bad)); Check(!system.Initialize(fixture));
    }
    for (const auto bad : {nlohmann::json{{"version","wrong"}},nlohmann::json{{"version",1.5}},
            nlohmann::json{{"version",1},{"center",0}}}) {
        Check(JsonFile::Save(fixture,bad)); Check(!system.Initialize(fixture)); Check(!system.RequestStart());
    }
    config["radii"] = {0,3};
    Check(JsonFile::Save(fixture,config)); Check(!system.Initialize(fixture));
    std::cout << "PASS pond projection361 points / " << clicks << " start clicks / sound time / exact-once / presented cover / held release / invalid config\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
