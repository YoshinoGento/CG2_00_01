#include "title/TitlePresentationSystem.h"
#include "title/TitleCelestialGeometry.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Check(bool value) { if (!value) throw std::runtime_error("title presentation assertion failed"); }
bool Near(float a, float b) { return std::abs(a-b) < 0.002f; }
}
int main() {
    const auto cameraWorld = MatrixMath::MakeAffineMatrix({1,1,1},{.10f,-.45f,0},{6.5f,3.5f,-6.5f});
    const auto projection = MatrixMath::MakePerspectiveFovMatrix(.72f,16.0f/9.0f,.1f,180);
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
    std::cout << "Title: farm, four phases, continuous celestial arcs/hidden turnarounds, wrap, invalid delta, immutable grid and single transition PASS\n";
}
