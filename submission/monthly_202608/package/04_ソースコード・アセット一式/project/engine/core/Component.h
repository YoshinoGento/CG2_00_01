#pragma once

namespace engine {

class GameObject;

class Component {
public:
    virtual ~Component() = default;

    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;
    Component(Component&&) = delete;
    Component& operator=(Component&&) = delete;

    GameObject* GetOwner();
    const GameObject* GetOwner() const;

    bool IsActive() const;
    void SetActive(bool active);

protected:
    Component() = default;

    virtual void OnStart() {}
    virtual void OnUpdate(float deltaTime) { (void)deltaTime; }
    virtual void OnDestroy() {}

private:
    friend class GameObject;

    void SetOwner(GameObject* owner);
    void Start();
    void Update(float deltaTime);
    void Destroy();

    GameObject* owner_ = nullptr;
    bool active_ = true;
    bool started_ = false;
    bool destroyed_ = false;
};

} // namespace engine
