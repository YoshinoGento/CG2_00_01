#include "editor/GamePlayEditorBridge.h"
namespace editor {
void GamePlayEditorBridge::BuildViewModel(GamePlayEditorViewModel& output) const {
    if (target_) target_->BuildViewModel(output);
    else output = {};
}
bool GamePlayEditorBridge::Execute(const GamePlayEditorCommand& command) { return target_ && target_->Execute(command); }
bool GamePlayEditorBridge::Execute(const FarmDocumentCommand& command) { return target_ && target_->Execute(command); }
bool GamePlayEditorBridge::Execute(const VisibilityEditorCommand& command) { return target_ && target_->Execute(command); }
bool GamePlayEditorBridge::Execute(const CameraEditorCommand& command) { return target_ && target_->Execute(command); }
bool GamePlayEditorBridge::Execute(const ObjectInspectorEditorCommand& command) { return target_ && target_->Execute(command); }
bool GamePlayEditorBridge::Execute(const ParticleEditorCommand& command) { return target_ && target_->Execute(command); }
bool GamePlayEditorBridge::Execute(const SimulationEditorCommand& command) { return target_ && target_->Execute(command); }
void GamePlayEditorBridge::SetViewportState(const Vector2& topLeft, const Vector2& size,
    const Vector2& pointer, bool hovered, bool focused) noexcept {
    if (target_) target_->SetViewportState(topLeft, size, pointer, hovered, focused);
}
Object3d* GamePlayEditorBridge::GetAnimationObjectForDebug() const noexcept {
    return target_ ? target_->GetAnimationObjectForDebug() : nullptr;
}
} // namespace editor
