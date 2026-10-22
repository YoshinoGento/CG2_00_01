#pragma once
#include "scene/BaseScene.h"
#include "math/Struct.h"
#include <memory>
#include <cstdint>
class GamePlaySessionSystem;
class Camera;
class FieldManager;
class SkyboxManager;
namespace editor { class GamePlayEditorBridge; }
class GamePlayScene final : public BaseScene {
public:
    GamePlayScene();
    ~GamePlayScene() override;
    void Initialize() override;
    InitializationTask InitializeIncrementally() override;
    void Finalize() override;
    void PrepareFixedUpdate() override;
    void OnScreenTransitionCovered() override;
    void FixedUpdate(float fixedDeltaTime) override;
    void Update() override;
    void Draw() override;
    editor::GamePlayEditorBridge& GetEditorBridge() noexcept;
    const editor::GamePlayEditorBridge& GetEditorBridge() const noexcept;
    const Camera* GetCamera() const noexcept;
    bool IsFarmGameMode() const noexcept;
    bool SetFarmGameMode(bool enabled);
    void RequestFarmLayoutLibrary() noexcept;
private:
    std::unique_ptr<GamePlaySessionSystem> session_;
};
