#pragma once
#include "math/Struct.h"
#include <cstdint>
#include <string>
#include <vector>

class SpriteCommon;

class BitmapFont {
public:
    bool Initialize(
        SpriteCommon* spriteCommon,
        const std::string& texturePath,
        const Vector2& glyphSize,
        int columns,
        const std::string& characters);
    bool InitializeFromJson(SpriteCommon* spriteCommon, const std::string& jsonPath);

    uint32_t GetTextureHandle() const { return textureHandle_; }
    Vector2 GetGlyphSize() const { return glyphSize_; }

    bool TryGetGlyphRect(char c, Vector2& outLeftTop, Vector2& outSize) const;
    bool TryGetGlyphRect(const std::string& glyph, Vector2& outLeftTop, Vector2& outSize) const;

private:
    SpriteCommon* spriteCommon_ = nullptr;
    uint32_t textureHandle_ = 0;
    Vector2 glyphSize_{};
    int columns_ = 0;
    std::string characters_;
    std::vector<std::string> glyphs_;
};
