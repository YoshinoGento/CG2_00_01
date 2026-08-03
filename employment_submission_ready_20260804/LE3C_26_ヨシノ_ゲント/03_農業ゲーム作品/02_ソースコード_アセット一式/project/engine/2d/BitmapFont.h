#pragma once
#include "2d/TextureManager.h"
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

    Texture2DHandle GetTextureHandle() const { return textureHandle_; }
    const std::string& GetTexturePath() const { return texturePath_; }
    Vector2 GetGlyphSize() const { return glyphSize_; }

    bool TryGetGlyphRect(char c, Vector2& outLeftTop, Vector2& outSize) const;
    bool TryGetGlyphRect(const std::string& glyph, Vector2& outLeftTop, Vector2& outSize) const;

private:
    SpriteCommon* spriteCommon_ = nullptr;
    Texture2DHandle textureHandle_{};
    std::string texturePath_;
    Vector2 glyphSize_{};
    int columns_ = 0;
    std::string characters_;
    std::vector<std::string> glyphs_;
};
