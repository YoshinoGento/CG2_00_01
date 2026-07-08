#include "BitmapFont.h"
#include "2d/SpriteCommon.h"
#include "base/Logger.h"
#include "io/JsonFile.h"

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

bool BitmapFont::Initialize(
    SpriteCommon* spriteCommon,
    const std::string& texturePath,
    const Vector2& glyphSize,
    int columns,
    const std::string& characters) {

    if (spriteCommon == nullptr) {
        Logger::Log("BitmapFont::Initialize failed. spriteCommon is null.");
        return false;
    }
    if (texturePath.empty()) {
        Logger::Log("BitmapFont::Initialize failed. texturePath is empty.");
        return false;
    }
    if (glyphSize.x <= 0.0f || glyphSize.y <= 0.0f) {
        Logger::Log("BitmapFont::Initialize failed. glyphSize must be positive.");
        return false;
    }
    if (columns <= 0) {
        Logger::Log("BitmapFont::Initialize failed. columns must be positive.");
        return false;
    }
    if (characters.empty()) {
        Logger::Log("BitmapFont::Initialize failed. characters is empty.");
        return false;
    }

    spriteCommon_ = spriteCommon;
    textureHandle_ = spriteCommon_->LoadTexture(texturePath);
    glyphSize_ = glyphSize;
    columns_ = columns;
    characters_ = characters;
    glyphs_ = SplitUtf8Glyphs(characters_);
    if (glyphs_.empty()) {
        Logger::Log("BitmapFont::Initialize failed. glyph list is empty.");
        return false;
    }

    return true;
}

bool BitmapFont::InitializeFromJson(SpriteCommon* spriteCommon, const std::string& jsonPath) {
    if (spriteCommon == nullptr) {
        Logger::Log("BitmapFont::InitializeFromJson failed. spriteCommon is null.");
        return false;
    }
    if (jsonPath.empty()) {
        Logger::Log("BitmapFont::InitializeFromJson failed. jsonPath is empty.");
        return false;
    }

    nlohmann::json json;
    if (!JsonFile::Load(jsonPath, json)) {
        Logger::Log("BitmapFont::InitializeFromJson failed. Could not load json: " + jsonPath);
        return false;
    }

    if (!json.contains("texture") || !json["texture"].is_string()) {
        Logger::Log("BitmapFont::InitializeFromJson failed. texture is missing or not a string: " + jsonPath);
        return false;
    }
    if (!json.contains("glyphWidth") || !json["glyphWidth"].is_number()) {
        Logger::Log("BitmapFont::InitializeFromJson failed. glyphWidth is missing or not a number: " + jsonPath);
        return false;
    }
    if (!json.contains("glyphHeight") || !json["glyphHeight"].is_number()) {
        Logger::Log("BitmapFont::InitializeFromJson failed. glyphHeight is missing or not a number: " + jsonPath);
        return false;
    }
    if (!json.contains("columns") || !json["columns"].is_number_integer()) {
        Logger::Log("BitmapFont::InitializeFromJson failed. columns is missing or not an integer: " + jsonPath);
        return false;
    }
    if (!json.contains("characters") || !json["characters"].is_string()) {
        Logger::Log("BitmapFont::InitializeFromJson failed. characters is missing or not a string: " + jsonPath);
        return false;
    }

    const std::string texturePath = json["texture"].get<std::string>();
    const float glyphWidth = json["glyphWidth"].get<float>();
    const float glyphHeight = json["glyphHeight"].get<float>();
    const int columns = json["columns"].get<int>();
    const std::string characters = json["characters"].get<std::string>();

    if (glyphWidth <= 0.0f) {
        Logger::Log("BitmapFont::InitializeFromJson failed. glyphWidth must be positive: " + jsonPath);
        return false;
    }
    if (glyphHeight <= 0.0f) {
        Logger::Log("BitmapFont::InitializeFromJson failed. glyphHeight must be positive: " + jsonPath);
        return false;
    }
    if (columns <= 0) {
        Logger::Log("BitmapFont::InitializeFromJson failed. columns must be positive: " + jsonPath);
        return false;
    }
    if (characters.empty()) {
        Logger::Log("BitmapFont::InitializeFromJson failed. characters is empty: " + jsonPath);
        return false;
    }

    return Initialize(spriteCommon, texturePath, { glyphWidth, glyphHeight }, columns, characters);
}

bool BitmapFont::TryGetGlyphRect(char c, Vector2& outLeftTop, Vector2& outSize) const {
    return TryGetGlyphRect(std::string(1, c), outLeftTop, outSize);
}

bool BitmapFont::TryGetGlyphRect(const std::string& glyph, Vector2& outLeftTop, Vector2& outSize) const {
    if (columns_ <= 0 || glyphSize_.x <= 0.0f || glyphSize_.y <= 0.0f) {
        return false;
    }
    if (glyph.empty()) {
        return false;
    }

    for (std::size_t index = 0; index < glyphs_.size(); ++index) {
        if (glyphs_[index] != glyph) {
            continue;
        }

        const int glyphIndex = static_cast<int>(index);
        outLeftTop = {
            static_cast<float>(glyphIndex % columns_) * glyphSize_.x,
            static_cast<float>(glyphIndex / columns_) * glyphSize_.y,
        };
        outSize = glyphSize_;
        return true;
    }

    return false;
}
