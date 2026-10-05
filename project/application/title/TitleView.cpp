#include "title/TitleView.h"
#include "io/Input.h"
#include "base/WinApp.h"
namespace title {
bool TitleView::Initialize(SpriteCommon* common) {
    if (!common) return false;
    auto logo = std::make_unique<Sprite>(), prompt = std::make_unique<Sprite>(), fade = std::make_unique<Sprite>();
    if (!logo->Initialize(common,"Resources/title/logo.png") ||
        !prompt->Initialize(common,"Resources/title/start.png") ||
        !fade->Initialize(common,"Resources/farm/white.png")) return false;
    common_ = common;
    logo->SetAnchorPoint({.5f,.5f}); logo->SetPosition(kLogoCenter); logo->SetSize(kLogoSize);
    prompt->SetAnchorPoint({.5f,.5f}); prompt->SetPosition(kStartCenter); prompt->SetSize(kStartSize);
    fade->SetPosition({0,0}); fade->SetSize({kWidth,kHeight});
    logo_ = std::move(logo); prompt_ = std::move(prompt); fade_ = std::move(fade);
    return true;
}
bool TitleView::IsStartRequested(const Input& input, const WinApp& window) const {
    if (input.TriggerKey(InputKey::Space) || input.TriggerKey(InputKey::Enter) ||
        input.TriggerGamepadButton(InputGamepadButton::A)) return true;
#ifdef USE_IMGUI
    // EditorShell supplies clicks in the displayed image's virtual coordinates.
    (void)window;
    return false;
#else
    const auto width = window.GetClientWidth(), height = window.GetClientHeight();
    if (!width || !height || !input.TriggerMouseButton(InputMouseButton::Left)) return false;
    const auto mouse = input.GetMousePosition();
    const float x = mouse.x*kWidth/static_cast<float>(width), y = mouse.y*kHeight/static_cast<float>(height);
    return HitTestStartButton({x,y});
#endif
}
void TitleView::Draw(const Frame& frame) {
    if (!common_) return;
    common_->PreDraw();
    logo_->SetPosition({kLogoCenter.x,kLogoCenter.y+frame.logoOffsetY});
    logo_->SetSize({kLogoSize.x*frame.logoScale,kLogoSize.y*frame.logoScale});
    logo_->Update(); logo_->Draw();
    prompt_->SetColor({1,1,1,frame.promptAlpha}); prompt_->Update(); prompt_->Draw();
    if (frame.fadeAlpha > 0) {
        fade_->SetColor({0,0,0,frame.fadeAlpha}); fade_->Update(); fade_->Draw();
    }
}
}
