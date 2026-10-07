#include "scene/SceneTransitionSystem.h"
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {
void Check(bool value, std::source_location at = std::source_location::current()) {
    if (!value) throw std::runtime_error("transition assertion at line "+std::to_string(at.line()));
}
}
int main() {
    scene::SceneTransitionSystem system;
    Check(!system.IsActive() && !system.CanSwap());
    system.NotifyCoverPresented();
    Check(!system.CanSwap() && !system.BeginReveal());
    Check(system.Begin() && !system.Begin());
    for (float invalid : {-1.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        system.Update(invalid,true);
        Check(system.GetFrame().progress == 0 && !system.CanSwap());
    }
    system.Update(.2f,true);
    Check(system.GetFrame().progress > 0 && system.GetFrame().progress < 1);
    system.NotifyCoverPresented();
    Check(!system.CanSwap());
    system.Update(.2f,true);
    Check(system.GetFrame().progress == 1 && !system.CanSwap());
    system.NotifyCoverPresented();
    Check(system.CanSwap() && system.BeginReveal());
    Check(!system.CanSwap() && !system.BeginReveal() && !system.Begin());
    Check(system.BlocksIncomingUpdate() && system.GetFrame().progress == 0);
    system.Update(.2f,false);
    Check(system.GetFrame().progress > 0 && system.GetFrame().progress < 1);
    system.Update(.2f,true);
    Check(system.GetFrame().phase == scene::TransitionPhase::WaitingForRelease);
    Check(system.IsActive() && system.GetFrame().progress == 1);
    for (int i=0; i<100; ++i) system.Update(.016f,false);
    Check(system.BlocksIncomingUpdate());
    system.Update(0,true);
    Check(!system.IsActive() && !system.BlocksIncomingUpdate());
    Check(system.Begin());
    system.Update(10000,true);
    Check(system.GetFrame().progress < 1); // Hitch cannot bypass the bounded cover.
    for (int run=0; run<100; ++run) {
        if (!system.IsActive()) Check(system.Begin());
        float previous = system.GetFrame().progress;
        while (system.GetFrame().progress < 1) {
            system.Update(.016f,true);
            Check(system.GetFrame().progress >= previous);
            previous = system.GetFrame().progress;
        }
        Check(!system.CanSwap());
        system.NotifyCoverPresented();
        Check(system.BeginReveal());
        previous = 0;
        while (system.GetFrame().phase == scene::TransitionPhase::Revealing) {
            system.Update(.016f,true);
            Check(system.GetFrame().progress >= previous);
            previous = system.GetFrame().progress;
        }
        Check(system.IsActive());
        system.Update(0,true);
        Check(!system.IsActive());
    }
    std::cout << "Scene transition: presented-cover gate, exact-once swap, bounded progress, held-input gate, 100 repeats PASS\n";
}
