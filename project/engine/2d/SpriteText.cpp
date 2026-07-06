#include "SpriteText.h"
#include "2d/BitmapFont.h"
#include "base/Logger.h"

void SpriteText::Initialize(SpriteCommon* spriteCommon, const BitmapFont* font) {
    if (spriteCommon == nullptr) {
        Logger::Log("SpriteText::Initialize failed. spriteCommon is null.");
        return;
    }
    if (font == nullptr) {
        Logger::Log("SpriteText::Initialize failed. font is null.");
        return;
    }

    spriteCommon_ = spriteCommon;
    font_ = font;
    RebuildGlyphs();
}

void SpriteText::SetText(const std::string& text) {
    if (text_ == text) {
        return;
    }

    text_ = text;
    RebuildGlyphs();
}

void SpriteText::SetPosition(const Vector2& position) {
    position_ = position;
    ApplyLayout();
}

void SpriteText::SetScale(float scale) {
    if (scale <= 0.0f) {
        Logger::Log("SpriteText::SetScale ignored. scale must be positive.");
        return;
    }

    scale_ = scale;
    ApplyLayout();
}

void SpriteText::SetCharacterSpacing(float spacing) {
    characterSpacing_ = spacing;
    ApplyLayout();
}

void SpriteText::SetColor(const Vector4& color) {
    color_ = color;
    for (std::size_t i = 0; i < activeGlyphCount_; ++i) {
        glyphSprites_[i]->SetColor(color_);
    }
}

void SpriteText::Update() {
    for (std::size_t i = 0; i < activeGlyphCount_; ++i) {
        glyphSprites_[i]->Update();
    }
}

void SpriteText::Draw() {
    for (std::size_t i = 0; i < activeGlyphCount_; ++i) {
        glyphSprites_[i]->Draw();
    }
}

void SpriteText::EnsureGlyphSprite(std::size_t index) {
    if (spriteCommon_ == nullptr || font_ == nullptr) {
        return;
    }

    while (glyphSprites_.size() <= index) {
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(spriteCommon_, font_->GetTextureHandle());
        glyphSprites_.push_back(std::move(sprite));
    }
}

void SpriteText::RebuildGlyphs() {
    activeGlyphCount_ = 0;

    if (spriteCommon_ == nullptr || font_ == nullptr) {
        if (!text_.empty()) {
            Logger::Log("SpriteText::SetText ignored. SpriteText is not initialized.");
        }
        return;
    }

    bool loggedUnsupported = false;
    for (std::size_t textIndex = 0; textIndex < text_.size(); ++textIndex) {
        const char c = text_[textIndex];
        Vector2 glyphLeftTop{};
        Vector2 glyphSize{};
        if (!font_->TryGetGlyphRect(c, glyphLeftTop, glyphSize)) {
            if (c != '\n' && !loggedUnsupported) {
                Logger::Log("SpriteText::SetText skipped unsupported character.");
                loggedUnsupported = true;
            }
            continue;
        }

        EnsureGlyphSprite(activeGlyphCount_);
        Sprite* sprite = glyphSprites_[activeGlyphCount_].get();
        sprite->SetTexture(font_->GetTextureHandle());
        sprite->SetTextureRect(glyphLeftTop, glyphSize);
        sprite->SetColor(color_);
        ++activeGlyphCount_;
    }

    ApplyLayout();
}

void SpriteText::ApplyLayout() {
    if (font_ == nullptr) {
        return;
    }

    const Vector2 glyphSize = font_->GetGlyphSize();
    const float glyphAdvance = glyphSize.x * scale_ + characterSpacing_;
    std::size_t glyphIndex = 0;
    for (std::size_t textIndex = 0; textIndex < text_.size() && glyphIndex < activeGlyphCount_; ++textIndex) {
        Vector2 glyphLeftTop{};
        Vector2 glyphRectSize{};
        if (!font_->TryGetGlyphRect(text_[textIndex], glyphLeftTop, glyphRectSize)) {
            continue;
        }

        Sprite* sprite = glyphSprites_[glyphIndex].get();
        sprite->SetPosition({
            position_.x + static_cast<float>(glyphIndex) * glyphAdvance,
            position_.y,
        });
        sprite->SetSize({
            glyphRectSize.x * scale_,
            glyphRectSize.y * scale_,
        });
        sprite->SetColor(color_);
        ++glyphIndex;
    }
}
