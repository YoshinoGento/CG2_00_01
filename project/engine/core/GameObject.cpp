#include "core/GameObject.h"

namespace engine {

GameObject::GameObject(std::string name)
    : name_(std::move(name)) {
}

GameObject::~GameObject() {
    Destroy();
}

const std::string& GameObject::GetName() const {
    return name_;
}

void GameObject::SetName(std::string name) {
    name_ = std::move(name);
}

Transform& GameObject::GetTransform() {
    return transform_;
}

const Transform& GameObject::GetTransform() const {
    return transform_;
}

bool GameObject::IsActive() const {
    return active_;
}

void GameObject::SetActive(bool active) {
    active_ = active;
}

bool GameObject::IsDestroyRequested() const {
    return destroyRequested_;
}

void GameObject::MarkDestroy() {
    destroyRequested_ = true;
}

void GameObject::Start() {
    if (started_ || destroyed_ || destroyRequested_) {
        return;
    }

    started_ = true;
    for (std::size_t index = 0; index < components_.size(); ++index) {
        components_[index]->Start();
        if (destroyRequested_) {
            break;
        }
    }
}

void GameObject::Update(float deltaTime) {
    if (!active_ || destroyed_ || destroyRequested_) {
        return;
    }

    Start();
    if (destroyRequested_) {
        return;
    }

    for (std::size_t index = 0; index < components_.size(); ++index) {
        components_[index]->Update(deltaTime);
        if (destroyRequested_) {
            break;
        }
    }
}

void GameObject::Destroy() {
    if (destroyed_) {
        return;
    }

    destroyed_ = true;
    destroyRequested_ = true;
    for (std::unique_ptr<Component>& component : components_) {
        component->Destroy();
    }
}

} // namespace engine
