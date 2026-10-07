#include "title/TitleView.h"
namespace title {
bool TitleView::Initialize(SpriteCommon* common) {
    if (!common) return false;
    auto logo = std::make_unique<Sprite>(), prompt = std::make_unique<Sprite>(), fade = std::make_unique<Sprite>();
    if (!logo->Initialize(common,"Resources/title/logo.png") ||
        !prompt->Initialize(common,"Resources/title/start.png") ||
        !fade->Initialize(common,"Resources/farm/white.png")) return false;
    auto reflection = std::make_unique<TitleLogoRippleRenderer>();
    if (!reflection->Initialize(common->GetDxCommon(),TextureManager::GetInstance())) return false;
    common_ = common;
    logo->SetAnchorPoint({.5f,.5f}); logo->SetPosition(kLogoCenter); logo->SetSize(kLogoSize);
    prompt->SetAnchorPoint({.5f,.5f}); prompt->SetPosition(kStartCenter); prompt->SetSize(kStartSize);
    fade->SetPosition({0,0}); fade->SetSize({kWidth,kHeight});
    logo_ = std::move(logo); prompt_ = std::move(prompt); fade_ = std::move(fade);
    reflection_ = std::move(reflection);
    return true;
}
void TitleView::Draw(const Frame& frame) {
    if (!common_) return;
    common_->PreDraw();
    logo_->Update(); logo_->Draw();
    reflection_->Draw(frame);
    common_->PreDraw();
    prompt_->SetColor({1,1,1,frame.promptAlpha}); prompt_->Update(); prompt_->Draw();
    if (frame.fadeAlpha > 0) {
        fade_->SetColor({0,0,0,frame.fadeAlpha}); fade_->Update(); fade_->Draw();
    }
}
}
