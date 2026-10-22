#pragma once
#include "base/Framework.h"
#include <memory>

class EditorShell;
class PostEffectSystem;
class SceneFactory;

// Owns application startup, frame scheduling and scene lifetime.
class Game final : public Framework {
public:
    Game();
    ~Game() override;
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;
private:
    std::unique_ptr<SceneFactory> sceneFactory_;
    std::unique_ptr<PostEffectSystem> postEffectSystem_;
#ifdef USE_IMGUI
    std::unique_ptr<EditorShell> editorShell_;
#endif
};
