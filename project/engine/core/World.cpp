#include "core/World.h"

#include <algorithm>

namespace engine {

World::~World() {
    Clear();
}

GameObject& World::CreateObject(std::string name) {
    auto object = std::make_unique<GameObject>(std::move(name));
    GameObject& result = *object;
    objects_.push_back(std::move(object));
    return result;
}

bool World::DestroyObject(GameObject* object) {
    if (!object) {
        return false;
    }

    const auto found = std::find_if(objects_.begin(), objects_.end(), [object](const std::unique_ptr<GameObject>& candidate) {
        return candidate.get() == object;
    });
    if (found == objects_.end() || (*found)->IsDestroyRequested()) {
        return false;
    }

    (*found)->MarkDestroy();
    return true;
}

GameObject* World::FindObject(std::string_view name) {
    for (const std::unique_ptr<GameObject>& object : objects_) {
        if (!object->IsDestroyRequested() && object->GetName() == name) {
            return object.get();
        }
    }
    return nullptr;
}

const GameObject* World::FindObject(std::string_view name) const {
    for (const std::unique_ptr<GameObject>& object : objects_) {
        if (!object->IsDestroyRequested() && object->GetName() == name) {
            return object.get();
        }
    }
    return nullptr;
}

std::size_t World::GetObjectCount() const {
    return objects_.size();
}

void World::Update(float deltaTime) {
    for (const std::unique_ptr<GameObject>& object : objects_) {
        object->Update(deltaTime);
    }
    RemoveDestroyedObjects();
}

void World::Clear() {
    for (std::unique_ptr<GameObject>& object : objects_) {
        object->Destroy();
    }
    objects_.clear();
}

void World::RemoveDestroyedObjects() {
    std::erase_if(objects_, [](const std::unique_ptr<GameObject>& object) {
        return object->IsDestroyRequested();
    });
}

} // namespace engine
