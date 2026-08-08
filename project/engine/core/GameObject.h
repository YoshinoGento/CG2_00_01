#pragma once

#include "core/Component.h"
#include "math/Transform.h"

#include <cassert>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace engine {

class World;

class GameObject final {
public:
    explicit GameObject(std::string name = "GameObject");
    ~GameObject();

    GameObject(const GameObject&) = delete;
    GameObject& operator=(const GameObject&) = delete;
    GameObject(GameObject&&) = delete;
    GameObject& operator=(GameObject&&) = delete;

    const std::string& GetName() const;
    void SetName(std::string name);

    Transform& GetTransform();
    const Transform& GetTransform() const;

    bool IsActive() const;
    void SetActive(bool active);

    bool IsDestroyRequested() const;
    void MarkDestroy();

    template<class T, class... Args>
    T& AddComponent(Args&&... args) {
        static_assert(std::is_base_of_v<Component, T>, "T must derive from engine::Component.");
        assert(!destroyRequested_ && "Cannot add a component to an object pending destruction.");

        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T& result = *component;
        component->SetOwner(this);
        components_.push_back(std::move(component));

        if (started_) {
            result.Start();
        }
        return result;
    }

    template<class T>
    T* GetComponent() {
        static_assert(std::is_base_of_v<Component, T>, "T must derive from engine::Component.");
        for (const std::unique_ptr<Component>& component : components_) {
            if (T* result = dynamic_cast<T*>(component.get())) {
                return result;
            }
        }
        return nullptr;
    }

    template<class T>
    const T* GetComponent() const {
        static_assert(std::is_base_of_v<Component, T>, "T must derive from engine::Component.");
        for (const std::unique_ptr<Component>& component : components_) {
            if (const T* result = dynamic_cast<const T*>(component.get())) {
                return result;
            }
        }
        return nullptr;
    }

private:
    friend class World;

    void Start();
    void Update(float deltaTime);
    void Destroy();

    std::string name_;
    Transform transform_ = {
        {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
    };
    std::vector<std::unique_ptr<Component>> components_;
    bool active_ = true;
    bool started_ = false;
    bool destroyRequested_ = false;
    bool destroyed_ = false;
};

} // namespace engine
