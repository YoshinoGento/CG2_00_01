#pragma once

#include "core/GameObject.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace engine {

class World final {
public:
    World() = default;
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = delete;
    World& operator=(World&&) = delete;

    GameObject& CreateObject(std::string name = "GameObject");
    bool DestroyObject(GameObject* object);

    GameObject* FindObject(std::string_view name);
    const GameObject* FindObject(std::string_view name) const;

    std::size_t GetObjectCount() const;

    void Update(float deltaTime);
    void Clear();

private:
    void RemoveDestroyedObjects();

    std::vector<std::unique_ptr<GameObject>> objects_;
};

} // namespace engine
