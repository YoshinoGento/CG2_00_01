#include "title/TitlePresentationSystem.h"
#include "title/TitleCelestialGeometry.h"
#include "title/TitleCropPresentation.h"
#include "title/TitleLogoRipplePoints.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <source_location>

namespace {
void Check(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::cerr << "Title assertion at line " << location.line() << std::endl;
    throw std::runtime_error("title presentation assertion failed");
}
bool Near(float a, float b) { return std::abs(a-b) < 0.002f; }
}
int main() {
    Check(title::HitTestStartButton(title::kStartCenter));
    Check(title::HitTestStartButton({500,610}));
    Check(title::HitTestStartButton({780,690}));
    for (const auto resolution : {Vector2{1600,900},Vector2{1280,720},Vector2{1920,1080}}) {
        Check(title::HitTestStartButtonInViewport({resolution.x*.5f,resolution.y*650/720},resolution));
        Check(!title::HitTestStartButtonInViewport({resolution.x*.8f,resolution.y*.9f},resolution));
    }
    Check(!title::HitTestStartButtonInViewport({640,650},{0,720}));
    Check(!title::HitTestStartButtonInViewport({640,650},{1280,std::numeric_limits<float>::infinity()}));
    for (const auto point : {Vector2{499,650}, Vector2{781,650}, Vector2{640,609},
            Vector2{640,691}, Vector2{0,0}, Vector2{640,std::numeric_limits<float>::quiet_NaN()},
            Vector2{std::numeric_limits<float>::infinity(),650}}) {
        Check(!title::HitTestStartButton(point));
    }
    const auto cameraWorld = MatrixMath::MakeAffineMatrix({1,1,1},title::kCameraRotation,title::kCameraPosition);
    const auto projection = MatrixMath::MakePerspectiveFovMatrix(title::kCameraFovY,title::kWidth/title::kHeight,.1f,180);
    float previousExposure = .25f;
    title::TitlePresentationSystem system;
    Check(system.Initialize());
    const auto generation = system.GetFarm().GetGeneration();
    Check(system.GetFarm().GetTileCount() == 20);
    Check(system.GetFarm().GetTile(2)->feature == farm::FarmTileFeature::WaterSource);
    for (int row = 1; row < 4; ++row) {
        const auto* upstream = system.GetFarm().GetTile((row-1)*5+2);
        const auto* downstream = system.GetFarm().GetTile(row*5+2);
        Check(downstream->feature == farm::FarmTileFeature::Canal);
        Check(upstream->heightLevel >= downstream->heightLevel);
    }
    const auto initial = system.GetFrame();
    std::array<bool, title::kTitleTileCount> sawGrowth{}, sawMature{}, sawEmpty{};
    std::array<int, title::kTitleTileCount> clearCounts{};
    system.Update(std::numeric_limits<float>::quiet_NaN());
    system.Update(std::numeric_limits<float>::infinity());
    system.Update(-1.0f);
    Check(system.GetFrame().skyBlend == initial.skyBlend);
    for (int phase = 0; phase < 4; ++phase) {
        const auto frame = system.GetFrame();
        Check(frame.skyFrom == static_cast<std::size_t>(phase));
        Check(frame.skyTo == (frame.skyFrom + 1) % 4);
        Check(Near(frame.skyBlend,0.0f));
        if (phase == 1) Check(Near(frame.groundExposure,.82f));
        if (phase == 3) Check(Near(frame.groundExposure,.10f));
        if (phase == 1) {
            Check(Near(frame.sun.center.x, .50f) && Near(frame.sun.center.y, .10f));
            Check(Near(frame.moon.center.x, .50f) && Near(frame.moon.center.y, 1.06f));
        } else if (phase == 3) {
            Check(Near(frame.moon.center.x, .50f) && Near(frame.moon.center.y, .10f));
            Check(Near(frame.sun.center.x, .50f) && Near(frame.sun.center.y, 1.06f));
        } else {
            // Both discs are below the terrain silhouette before either rises again.
            Check(frame.sun.center.y-.12f > .44f && frame.moon.center.y-.12f > .44f);
            Check(Near(frame.sun.center.x, phase == 0 ? .08f : .92f));
        }
        for (int step = 0; step < 40; ++step) {
            const auto previous = system.GetFrame();
            system.Update(0.25f);
            const auto current = system.GetFrame();
            Check(Near(current.promptAlpha,.82f+.18f*std::cos((phase*10+(step+1)*.25f)*
                std::numbers::pi_v<float>*2/2.5f)));
            for (const auto& ripple : current.logoRipples) {
                Check(std::isfinite(ripple.alpha) && ripple.alpha >= 0 && ripple.alpha <= .86f);
                Check(std::isfinite(ripple.radius) && ripple.radius >= 0);
            }
            for (std::size_t i=0; i<current.crops.size(); ++i) {
                const auto& crop = current.crops[i];
                Check(std::isfinite(crop.growth) && crop.growth >= 0 && crop.growth <= 1);
                const auto* tile = system.GetFarm().GetTile(static_cast<int>(i));
                if (!farm::IsPlantableCrop(tile->crop)) { Check(!crop.visible); continue; }
                sawGrowth[i] = sawGrowth[i] || (crop.visible && crop.growth < .5f);
                sawMature[i] = sawMature[i] || (crop.visible && crop.growth == 1);
                sawEmpty[i] = sawEmpty[i] || !crop.visible;
                if (previous.crops[i].visible && !crop.visible) ++clearCounts[i];
                farm::FarmTileVisualData visual;
                visual.valid = true; visual.center = {1,.54f,6}; visual.cropAnchor = visual.center;
                visual.crop = tile->crop;
                const auto parts = title::BuildTitleCropParts(visual,crop);
                if (!crop.visible) Check(parts.count == 0);
                else Check(parts.count >= 1 && parts.count <= 2);
                for (std::size_t part=0; part<parts.count; ++part) {
                    const auto& mesh = parts.parts[part];
                    Check(std::isfinite(mesh.position.y) && mesh.scale.x > .0001f && mesh.scale.y > .0001f);
                    if (mesh.shape == farm::FarmMeshShape::Carrot) Check(mesh.position.y < visual.center.y);
                    if (mesh.shape == farm::FarmMeshShape::TomatoStems || mesh.shape == farm::FarmMeshShape::PumpkinVines)
                        Check(Near(mesh.position.y,visual.center.y+.02f));
                }
            }
            Check(current.skyFrom < 4 && current.skyTo < 4);
            Check(current.skyBlend >= 0 && current.skyBlend <= 1);
            Check(std::isfinite(current.skyYaw) && current.skyYaw >= 0 && current.skyYaw < 1);
            Check(std::isfinite(current.cloudLift) && std::abs(current.cloudLift) <= .025f);
            Check(current.nightAmount >= 0 && current.nightAmount <= 1);
            for (const auto& body : {current.sun, current.moon}) {
                Check(std::isfinite(body.center.x) && std::isfinite(body.center.y));
                Check(body.center.x >= .079f && body.center.x <= .921f);
                Check(body.center.y >= .099f && body.center.y <= 1.061f);
            }
            Check(Near(current.sun.center.x+current.moon.center.x, 1.0f));
            Check(Near(current.sun.center.y+current.moon.center.y, 1.16f));
            if (std::abs(current.sun.center.y-.58f) < .02f) {
                Check(current.sun.center.y-.12f > .44f);
                Check(current.moon.center.y-.12f > .44f);
            }
            Check(std::abs(current.sun.center.x-previous.sun.center.x) < .02f);
            Check(std::abs(current.sun.center.y-previous.sun.center.y) < .02f);
            Check(std::abs(current.moon.center.x-previous.moon.center.x) < .02f);
            Check(std::abs(current.moon.center.y-previous.moon.center.y) < .02f);
            if (phase < 2) Check(current.sun.center.x >= previous.sun.center.x-.0001f);
            else Check(current.moon.center.x >= previous.moon.center.x-.0001f);
            const auto sun = title::CelestialDirection(current.sun,projection,cameraWorld);
            const auto moon = title::CelestialDirection(current.moon,projection,cameraWorld);
            const auto lighting = title::MakeCelestialLighting(sun,moon,current.groundExposure);
            Check(std::isfinite(lighting.direction.y) && lighting.direction.y < 0);
            Check(lighting.shadowStrength >= 0 && lighting.shadowStrength <= .70f);
            Check(Near(MatrixMath::Length(sun),1) && Near(MatrixMath::Length(lighting.direction),1));
            if (sun.y <= .05f) Check(lighting.shadowStrength == 0);
            else Check(MatrixMath::Dot(sun,lighting.direction) < -.9999f);
            if (phase >= 2) Check(lighting.shadowStrength == 0);
            auto movedCamera = cameraWorld;
            movedCamera.m[3][0] += 100;
            Check(Near(title::CelestialDirection(current.sun,projection,movedCamera).x,sun.x));
            Check(std::isfinite(lighting.intensity) && lighting.intensity >= 0 && lighting.intensity <= 1/.18f);
            const float received = lighting.intensity*(.18f+.82f*(-lighting.direction.y));
            Check(Near(received,current.groundExposure));
            Check(std::abs(received-previousExposure) < .023f);
            if (phase == 0 || phase == 3) Check(received >= previousExposure-.0001f);
            else Check(received <= previousExposure+.0001f);
            previousExposure = received;
            Check(std::isfinite(current.waterPhase) && current.waterPhase >= 0 && current.waterPhase < 1);
            for (const auto& star : current.stars) Check(star.alpha >= 0 && star.alpha <= 1);
        }
    }
    const auto wrapped = system.GetFrame();
    Check(wrapped.skyFrom == initial.skyFrom && Near(wrapped.waterPhase, initial.waterPhase));
    Check(Near(wrapped.lightColor.x,initial.lightColor.x));
    Check(Near(wrapped.windAngle,initial.windAngle));
    Check(Near(wrapped.sun.center.x,initial.sun.center.x));
    Check(Near(wrapped.sun.center.y,initial.sun.center.y));
    Check(Near(wrapped.moon.center.x,initial.moon.center.x));
    Check(Near(wrapped.moon.center.y,initial.moon.center.y));
    Check(system.GetFarm().GetGeneration() == generation);
    for (std::size_t i=0; i<wrapped.crops.size(); ++i) {
        Check(Near(wrapped.crops[i].growth,initial.crops[i].growth));
        Check(wrapped.crops[i].visible == initial.crops[i].visible);
        if (farm::IsPlantableCrop(system.GetFarm().GetTile(static_cast<int>(i))->crop))
            Check(sawGrowth[i] && sawMature[i] && sawEmpty[i] && clearCounts[i] == 4);
    }
    farm::FarmTileVisualData visual;
    visual.valid = true; visual.crop = farm::CropType::Carrot;
    const title::CropFrame mature{1,true}, cleared{1,false};
    const auto buried = title::BuildTitleCropParts(visual,mature);
    Check(buried.count == 2 && buried.parts[0].position.y < 0);
    visual.cropStage = farm::FarmCropGrowthStage::Ready;
    const auto shared = farm::BuildFarmCropMeshParts(visual,1.0f);
    for (std::size_t i=0; i<buried.count; ++i) {
        Check(buried.parts[i].position.y == shared.parts[i].position.y);
        Check(buried.parts[i].scale.y == shared.parts[i].scale.y);
    }
    Check(title::BuildTitleCropParts(visual,cleared).count == 0);
    title::TitlePresentationSystem rain, identicalRain, differentRain;
    Check(rain.Initialize(123,987) && identicalRain.Initialize(123,987) && differentRain.Initialize(123,654));
    constexpr float rainDelta = .03125f;
    std::array<bool,title::kLogoRipplePoints.size()> visited{};
    std::uint32_t lastId = 0;
    Vector2 lastCenter{};
    float lastBirth = 0, minimumGap = 10, maximumGap = 0, idle = 0;
    bool distinctSeed = false, sawOverlap = false, sawNightAttenuation = false;
    for (int step=1; step<=19200; ++step) {
        const auto before = rain.GetFrame();
        rain.Update(rainDelta); identicalRain.Update(rainDelta); differentRain.Update(rainDelta);
        const auto frame = rain.GetFrame(), twinFrame = identicalRain.GetFrame(), otherFrame = differentRain.GetFrame();
        int active = 0, births = 0;
        for (std::size_t i=0; i<frame.logoRipples.size(); ++i) {
            const auto& ripple = frame.logoRipples[i];
            const auto& twinRipple = twinFrame.logoRipples[i];
            Check(ripple.id == twinRipple.id && ripple.radius == twinRipple.radius && ripple.alpha == twinRipple.alpha);
            Check(ripple.center.x == twinRipple.center.x && ripple.center.y == twinRipple.center.y);
            distinctSeed = distinctSeed || ripple.id != otherFrame.logoRipples[i].id ||
                ripple.center.x != otherFrame.logoRipples[i].center.x;
            Check(std::isfinite(ripple.radius) && std::isfinite(ripple.alpha));
            Check(ripple.alpha >= 0 && ripple.alpha <= .86f-.22f*frame.nightAmount+.0001f);
            if (!ripple.id) { Check(ripple.alpha == 0); continue; }
            ++active;
            sawNightAttenuation = sawNightAttenuation || (frame.nightAmount > .9f && ripple.alpha > .5f && ripple.alpha < .67f);
            bool validPoint = false;
            for (std::size_t p=0; p<title::kLogoRipplePoints.size(); ++p) {
                const auto point = title::kLogoRipplePoints[p];
                if (ripple.center.x == point.x && ripple.center.y == point.y) { visited[p] = true; validPoint = true; }
            }
            Check(validPoint);
            if (ripple.id == before.logoRipples[i].id) Check(ripple.radius >= before.logoRipples[i].radius);
            if (ripple.id <= lastId) continue;
            ++births;
            Check(ripple.id == lastId+1);
            const float seconds = step*rainDelta;
            if (lastId == 0) Check(seconds >= .9f && seconds <= 1.5f+rainDelta);
            else {
                const float gap = seconds-lastBirth;
                Check(gap >= title::kLogoRippleMinimumInterval-rainDelta && gap <= title::kLogoRippleMaximumInterval+rainDelta);
                Check(ripple.center.x != lastCenter.x || ripple.center.y != lastCenter.y);
                minimumGap = (std::min)(minimumGap,gap); maximumGap = (std::max)(maximumGap,gap);
            }
            lastId = ripple.id; lastCenter = ripple.center; lastBirth = seconds;
        }
        Check(active <= 2 && births <= 1);
        sawOverlap = sawOverlap || active == 2;
        idle = active ? 0 : idle+rainDelta;
        Check(idle <= 1.5f+rainDelta);
        Check(frame.skyYaw == otherFrame.skyYaw); // Ripple seeds never consume the wind stream.
        const auto repeated = rain.GetFrame();
        Check(repeated.logoRipples[0].radius == frame.logoRipples[0].radius);
    }
    int visitedCount = 0;
    for (bool seen : visited) visitedCount += seen ? 1 : 0;
    Check(lastId >= 142 && lastId <= 231 && visitedCount >= 35);
    Check(maximumGap-minimumGap > .7f && distinctSeed && sawOverlap && sawNightAttenuation);
    const auto validRain = rain.GetFrame();
    for (const float bad : {0.0f,-1.0f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) rain.Update(bad);
    Check(rain.GetFrame().logoRipples[0].radius == validRain.logoRipples[0].radius);
    Check(rain.Initialize(123,987));
    for (const auto& ripple : rain.GetFrame().logoRipples) Check(ripple.id == 0 && ripple.alpha == 0);
    title::TitlePresentationSystem coarseRain, fineRain;
    Check(coarseRain.Initialize(123,789) && fineRain.Initialize(123,789));
    for (int step=0; step<4800; ++step) {
        coarseRain.Update(.125f);
        for (int sub=0; sub<4; ++sub) fineRain.Update(.03125f);
        const auto coarse = coarseRain.GetFrame(), fine = fineRain.GetFrame();
        for (std::size_t i=0; i<coarse.logoRipples.size(); ++i) {
            Check(coarse.logoRipples[i].id == fine.logoRipples[i].id);
            Check(coarse.logoRipples[i].center.x == fine.logoRipples[i].center.x);
            Check(std::abs(coarse.logoRipples[i].radius-fine.logoRipples[i].radius) < .1f);
        }
    }
    for (std::uint32_t seed=1; seed<=12; ++seed) {
        Check(rain.Initialize(123,seed));
        std::uint32_t id = 0;
        float seconds = 0, birth = 0;
        for (int step=0; step<1200; ++step) {
            const float delta = step%3 == 0 ? .25f : (step%3 == 1 ? .0625f : .125f);
            rain.Update(delta); seconds += delta;
            const auto frame = rain.GetFrame();
            int births = 0;
            for (const auto& ripple : frame.logoRipples) {
                if (ripple.id <= id) continue;
                Check(ripple.id == id+1);
                if (id) Check(seconds-birth >= title::kLogoRippleMinimumInterval-.25f &&
                    seconds-birth <= title::kLogoRippleMaximumInterval+.25f);
                birth = seconds; id = ripple.id; ++births;
            }
            Check(births <= 1);
        }
        Check(id > 35);
    }
    for (const auto invalid : {title::CropFrame{-1,true},
        title::CropFrame{std::numeric_limits<float>::quiet_NaN(),true},
        title::CropFrame{2,true}}) Check(title::BuildTitleCropParts(visual,invalid).count == 0);
    Check(!system.ConsumeStart());
    system.RequestStart();
    system.Update(0.25f);
    system.RequestStart();
    for (int i=0; i<3; ++i) system.Update(0.25f);
    Check(system.ConsumeStart());
    Check(!system.ConsumeStart());
    Check(system.GetFrame().fadeAlpha == 1.0f);
    Check(system.Initialize());
    system.Update(10000.0f);
    Check(system.GetFrame().skyFrom == 0 && system.GetFrame().skyBlend < 0.01f);
    title::TitlePresentationSystem twin, other;
    Check(system.Initialize(123) && twin.Initialize(123) && other.Initialize(456));
    float previousOffset = 0;
    for (int i=0; i<4800; ++i) {
        system.Update(.016f); twin.Update(.016f); other.Update(.016f);
        const auto frame = system.GetFrame();
        const float difference = std::abs(frame.skyYaw-previousOffset);
        Check((std::min)(difference,1-difference) < .0001f);
        Check(frame.skyYaw == twin.GetFrame().skyYaw);
        previousOffset = frame.skyYaw;
    }
    Check(std::abs(system.GetFrame().skyYaw-other.GetFrame().skyYaw) > .001f);
    auto invalidProjection = projection;
    invalidProjection.m[0][0] = 0;
    Check(title::CelestialDirection(initial.sun,invalidProjection,cameraWorld).y == -1);
    invalidProjection.m[0][0] = std::numeric_limits<float>::quiet_NaN();
    Check(title::CelestialDirection(initial.sun,invalidProjection,cameraWorld).y == -1);
    const auto invalidLighting = title::MakeCelestialLighting({0,std::numeric_limits<float>::quiet_NaN(),0},{0,0,0},.1f);
    Check(invalidLighting.shadowStrength == 0 && invalidLighting.direction.y == -1);
    Check(title::MakeCelestialLighting({0,1,0},{0,-1,0},std::numeric_limits<float>::quiet_NaN()).intensity == 0);
    std::cout << "Title: bounded seeded rain, glyph centers, max2, independent wind, 600s no drought/burst, sky/crops/blink/start PASS\n";
}
