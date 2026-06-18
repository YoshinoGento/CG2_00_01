#pragma once

#include "math/Matrix.h"

#include <string>
#include <vector>

class RuntimeTextTextureGenerator {
public:
    struct TextTextureRequest {
        std::string id;
        std::string text;
        std::string font;
        std::string output;
        float fontSize = 64.0f;
        Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        Vector4 shadowColor = { 0.0f, 0.0f, 0.0f, 0.6f };
        Vector2 shadowOffset = { 4.0f, 4.0f };
        Vector2 padding = { 32.0f, 20.0f };
        bool overwriteIfExists = false;
    };

    struct GenerateReport {
        bool success = true;
        std::vector<std::string> messages;
    };

    static bool GenerateTextTexture(const TextTextureRequest& request, std::string* errorMessage = nullptr);
    static GenerateReport GenerateFromJson(const std::string& jsonPath);
};
