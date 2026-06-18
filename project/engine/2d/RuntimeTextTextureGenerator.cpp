#include "RuntimeTextTextureGenerator.h"

#include "base/Logger.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <gdiplus.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

#pragma comment(lib, "gdiplus.lib")

namespace {
std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length);
    return result;
}

std::string ReadAllText(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

bool ExtractString(const std::string& objectText, const char* key, std::string& outValue) {
    const std::regex pattern(std::string("\"") + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (!std::regex_search(objectText, match, pattern)) {
        return false;
    }
    outValue = match[1].str();
    return true;
}

bool ExtractFloat(const std::string& objectText, const char* key, float& outValue) {
    const std::regex pattern(std::string("\"") + key + R"("\s*:\s*(-?[0-9]+(?:\.[0-9]+)?))");
    std::smatch match;
    if (!std::regex_search(objectText, match, pattern)) {
        return false;
    }
    outValue = std::stof(match[1].str());
    return true;
}

bool ExtractBool(const std::string& objectText, const char* key, bool& outValue) {
    const std::regex pattern(std::string("\"") + key + R"("\s*:\s*(true|false))");
    std::smatch match;
    if (!std::regex_search(objectText, match, pattern)) {
        return false;
    }
    outValue = match[1].str() == "true";
    return true;
}

std::vector<float> ParseFloatArray(const std::string& arrayText) {
    std::vector<float> values;
    const std::regex numberPattern(R"(-?[0-9]+(?:\.[0-9]+)?)");
    for (auto it = std::sregex_iterator(arrayText.begin(), arrayText.end(), numberPattern);
        it != std::sregex_iterator();
        ++it) {
        values.push_back(std::stof((*it)[0].str()));
    }
    return values;
}

bool ExtractFloatArray(const std::string& objectText, const char* key, std::vector<float>& outValues) {
    const std::regex pattern(std::string("\"") + key + R"("\s*:\s*\[([^\]]*)\])");
    std::smatch match;
    if (!std::regex_search(objectText, match, pattern)) {
        return false;
    }
    outValues = ParseFloatArray(match[1].str());
    return true;
}

std::vector<std::string> ExtractObjectsInTextTextures(const std::string& jsonText) {
    std::vector<std::string> objects;
    const size_t keyPos = jsonText.find("\"textTextures\"");
    if (keyPos == std::string::npos) {
        return objects;
    }
    const size_t arrayBegin = jsonText.find('[', keyPos);
    if (arrayBegin == std::string::npos) {
        return objects;
    }

    int arrayDepth = 0;
    int objectDepth = 0;
    size_t objectStart = std::string::npos;
    for (size_t i = arrayBegin; i < jsonText.size(); ++i) {
        const char c = jsonText[i];
        if (c == '[') {
            ++arrayDepth;
        } else if (c == ']') {
            --arrayDepth;
            if (arrayDepth <= 0) {
                break;
            }
        } else if (c == '{') {
            if (arrayDepth == 1 && objectDepth == 0) {
                objectStart = i;
            }
            ++objectDepth;
        } else if (c == '}') {
            --objectDepth;
            if (arrayDepth == 1 && objectDepth == 0 && objectStart != std::string::npos) {
                objects.push_back(jsonText.substr(objectStart, i - objectStart + 1));
                objectStart = std::string::npos;
            }
        }
    }
    return objects;
}

Gdiplus::Color ToGdiColor(const Vector4& color) {
    const auto toByte = [](float value) -> BYTE {
        return static_cast<BYTE>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    return Gdiplus::Color(toByte(color.w), toByte(color.x), toByte(color.y), toByte(color.z));
}

int GetEncoderClsid(const WCHAR* format, CLSID* outClsid) {
    UINT encoderCount = 0;
    UINT encoderBytes = 0;
    Gdiplus::GetImageEncodersSize(&encoderCount, &encoderBytes);
    if (encoderBytes == 0) {
        return -1;
    }

    std::vector<BYTE> buffer(encoderBytes);
    auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    Gdiplus::GetImageEncoders(encoderCount, encoderBytes, encoders);
    for (UINT i = 0; i < encoderCount; ++i) {
        if (wcscmp(encoders[i].MimeType, format) == 0) {
            *outClsid = encoders[i].Clsid;
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool ParseRequest(const std::string& objectText, RuntimeTextTextureGenerator::TextTextureRequest& request, std::string& errorMessage) {
    if (!ExtractString(objectText, "id", request.id)) {
        request.id = "unnamed";
    }
    if (!ExtractString(objectText, "text", request.text)) {
        errorMessage = "text is missing for text texture: " + request.id;
        return false;
    }
    ExtractString(objectText, "font", request.font);
    if (!ExtractString(objectText, "output", request.output)) {
        errorMessage = "output is missing for text texture: " + request.id;
        return false;
    }
    ExtractFloat(objectText, "fontSize", request.fontSize);
    ExtractBool(objectText, "overwrite", request.overwriteIfExists);

    std::vector<float> values;
    if (ExtractFloatArray(objectText, "color", values) && values.size() >= 4) {
        request.color = { values[0], values[1], values[2], values[3] };
    }
    if (ExtractFloatArray(objectText, "shadowColor", values) && values.size() >= 4) {
        request.shadowColor = { values[0], values[1], values[2], values[3] };
    }
    if (ExtractFloatArray(objectText, "shadowOffset", values) && values.size() >= 2) {
        request.shadowOffset = { values[0], values[1] };
    }
    if (ExtractFloatArray(objectText, "padding", values) && values.size() >= 2) {
        request.padding = { values[0], values[1] };
    }
    return true;
}
}

bool RuntimeTextTextureGenerator::GenerateTextTexture(const TextTextureRequest& request, std::string* errorMessage) {
    if (request.output.empty() || request.text.empty()) {
        if (errorMessage) {
            *errorMessage = "RuntimeTextTextureGenerator: text or output path is empty.";
        }
        return false;
    }

    const std::filesystem::path outputPath(request.output);
    if (std::filesystem::exists(outputPath) && !request.overwriteIfExists) {
        return true;
    }
    if (outputPath.has_parent_path()) {
        std::error_code errorCode;
        std::filesystem::create_directories(outputPath.parent_path(), errorCode);
        if (errorCode) {
            if (errorMessage) {
                *errorMessage = "Failed to create text texture directory: " + outputPath.parent_path().string();
            }
            return false;
        }
    }

    Gdiplus::GdiplusStartupInput startupInput{};
    ULONG_PTR gdiplusToken = 0;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &startupInput, nullptr) != Gdiplus::Ok) {
        if (errorMessage) {
            *errorMessage = "GDI+ startup failed.";
        }
        return false;
    }

    bool result = false;
    std::string localError;
    {
        if (!request.font.empty() && !std::filesystem::exists(request.font)) {
            Logger::Log("Text texture font not found. Fallback to Arial: " + request.font);
        }

        Gdiplus::FontFamily fallbackFamily(L"Arial");
        Gdiplus::Font font(&fallbackFamily, (std::max)(request.fontSize, 1.0f), Gdiplus::FontStyleBold, Gdiplus::UnitPixel);

        const std::wstring text = Utf8ToWide(request.text);
        Gdiplus::Bitmap measureBitmap(1, 1, PixelFormat32bppARGB);
        Gdiplus::Graphics measureGraphics(&measureBitmap);
        measureGraphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
        Gdiplus::StringFormat stringFormat;
        stringFormat.SetAlignment(Gdiplus::StringAlignmentNear);
        stringFormat.SetLineAlignment(Gdiplus::StringAlignmentNear);
        Gdiplus::RectF bounds{};
        measureGraphics.MeasureString(text.c_str(), -1, &font, Gdiplus::PointF(0.0f, 0.0f), &stringFormat, &bounds);

        const float shadowExtraX = std::fabs(request.shadowOffset.x);
        const float shadowExtraY = std::fabs(request.shadowOffset.y);
        const int width = (std::max)(1, static_cast<int>(std::ceil(bounds.Width + request.padding.x * 2.0f + shadowExtraX)));
        const int height = (std::max)(1, static_cast<int>(std::ceil(bounds.Height + request.padding.y * 2.0f + shadowExtraY)));

        Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppARGB);
        Gdiplus::Graphics graphics(&bitmap);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
        graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

        const float baseX = request.padding.x + (std::max)(0.0f, -request.shadowOffset.x);
        const float baseY = request.padding.y + (std::max)(0.0f, -request.shadowOffset.y);
        Gdiplus::SolidBrush shadowBrush(ToGdiColor(request.shadowColor));
        Gdiplus::SolidBrush textBrush(ToGdiColor(request.color));
        graphics.DrawString(
            text.c_str(),
            -1,
            &font,
            Gdiplus::PointF(baseX + request.shadowOffset.x, baseY + request.shadowOffset.y),
            &stringFormat,
            &shadowBrush);
        graphics.DrawString(
            text.c_str(),
            -1,
            &font,
            Gdiplus::PointF(baseX, baseY),
            &stringFormat,
            &textBrush);

        CLSID pngClsid{};
        if (GetEncoderClsid(L"image/png", &pngClsid) < 0) {
            localError = "PNG encoder was not found.";
        } else {
            const std::wstring output = Utf8ToWide(request.output);
            const Gdiplus::Status saveStatus = bitmap.Save(output.c_str(), &pngClsid, nullptr);
            if (saveStatus == Gdiplus::Ok) {
                result = true;
            } else {
                localError = "Failed to save generated text texture: " + request.output;
            }
        }
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    if (!result && errorMessage) {
        *errorMessage = localError;
    }
    return result;
}

RuntimeTextTextureGenerator::GenerateReport RuntimeTextTextureGenerator::GenerateFromJson(const std::string& jsonPath) {
    GenerateReport report{};
    const std::string jsonText = ReadAllText(jsonPath);
    if (jsonText.empty()) {
        report.success = false;
        report.messages.push_back("Text texture json was not found or empty: " + jsonPath);
        return report;
    }

    const std::vector<std::string> objects = ExtractObjectsInTextTextures(jsonText);
    if (objects.empty()) {
        report.success = false;
        report.messages.push_back("No textTextures entries in: " + jsonPath);
        return report;
    }

    for (const std::string& objectText : objects) {
        TextTextureRequest request{};
        std::string message;
        if (!ParseRequest(objectText, request, message)) {
            report.success = false;
            report.messages.push_back(message);
            continue;
        }

        if (!GenerateTextTexture(request, &message)) {
            report.success = false;
            report.messages.push_back(message);
        } else {
            report.messages.push_back("Generated or reused text texture: " + request.output);
        }
    }

    for (const std::string& message : report.messages) {
        Logger::Log(message);
    }
    return report;
}
