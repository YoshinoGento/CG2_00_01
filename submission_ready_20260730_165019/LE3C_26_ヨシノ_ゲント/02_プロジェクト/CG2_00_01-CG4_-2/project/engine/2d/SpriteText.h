#pragma once
#include "2d/Sprite.h"
#include "math/Struct.h"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class BitmapFont;

class SpriteText {
public:
    void Initialize(SpriteCommon* spriteCommon, const BitmapFont* font);

    void SetText(const std::string& text);
    void SetPosition(const Vector2& position);
    void SetScale(float scale);
    void SetCharacterSpacing(float spacing);
    void SetColor(const Vector4& color);

    void Update();
    void Draw();

private:
    void EnsureGlyphSprite(std::size_t index);
    void RebuildGlyphs();
    void ApplyLayout();

private:
    SpriteCommon* spriteCommon_ = nullptr;
    const BitmapFont* font_ = nullptr;

    std::string text_;
    Vector2 position_{};
    float scale_ = 1.0f;
    float characterSpacing_ = 0.0f;
    Vector4 color_{ 1.0f, 1.0f, 1.0f, 1.0f };

    std::vector<std::unique_ptr<Sprite>> glyphSprites_;
    std::size_t activeGlyphCount_ = 0;
};
