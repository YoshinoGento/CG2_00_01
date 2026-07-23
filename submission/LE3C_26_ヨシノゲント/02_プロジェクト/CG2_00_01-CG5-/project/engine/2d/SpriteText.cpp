#include "SpriteText.h"
#include "2d/BitmapFont.h"
#include "base/Logger.h"

#include <algorithm>

namespace {
    std::size_t GetUtf8GlyphLength(unsigned char leadByte)
    {
        if ((leadByte & 0x80) == 0x00) {
            return 1;
        }
        if ((leadByte & 0xE0) == 0xC0) {
            return 2;
        }
        if ((leadByte & 0xF0) == 0xE0) {
            return 3;
        }
        if ((leadByte & 0xF8) == 0xF0) {
            return 4;
        }
        return 1;
    }

    std::vector<std::string> SplitUtf8Glyphs(const std::string& text)
    {
        std::vector<std::string> glyphs;
        for (std::size_t index = 0; index < text.size();) {
            const std::size_t glyphLength = GetUtf8GlyphLength(static_cast<unsigned char>(text[index]));
            const std::size_t safeLength = (std::min)(glyphLength, text.size() - index);
            glyphs.push_back(text.substr(index, safeLength));
            index += safeLength;
        }
        return glyphs;
    }
}

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
        if (!sprite->Initialize(spriteCommon_, font_->GetTexturePath())) {
            return;
        }
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
    const std::vector<std::string> glyphs = SplitUtf8Glyphs(text_);
    for (const std::string& glyph : glyphs) {
        Vector2 glyphLeftTop{};
        Vector2 glyphSize{};
        if (!font_->TryGetGlyphRect(glyph, glyphLeftTop, glyphSize)) {
            if (glyph != "\n" && !loggedUnsupported) {
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
    const std::vector<std::string> glyphs = SplitUtf8Glyphs(text_);
    std::size_t glyphIndex = 0;
    for (const std::string& glyph : glyphs) {
        if (glyphIndex >= activeGlyphCount_) {
            break;
        }

        Vector2 glyphLeftTop{};
        Vector2 glyphRectSize{};
        if (!font_->TryGetGlyphRect(glyph, glyphLeftTop, glyphRectSize)) {
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
