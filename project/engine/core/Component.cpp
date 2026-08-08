#include "core/Component.h"

#include <cmath>

namespace engine {

GameObject* Component::GetOwner() {
    return owner_;
}

const GameObject* Component::GetOwner() const {
    return owner_;
}

bool Component::IsActive() const {
    return active_;
}

void Component::SetActive(bool active) {
    active_ = active;
}

void Component::SetOwner(GameObject* owner) {
    owner_ = owner;
}

void Component::Start() {
    if (started_ || destroyed_) {
        return;
    }

    started_ = true;
    OnStart();
}

void Component::Update(float deltaTime) {
    if (destroyed_) {
        return;
    }

    Start();
    if (!active_ || !std::isfinite(deltaTime) || deltaTime <= 0.0f) {
        return;
    }

    OnUpdate(deltaTime);
}

void Component::Destroy() {
    if (destroyed_) {
        return;
    }

    destroyed_ = true;
    OnDestroy();
    owner_ = nullptr;
}

} // namespace engine
