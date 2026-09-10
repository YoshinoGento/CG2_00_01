#include "farm/ui/FarmRuntimeUI.h"
#include <algorithm>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

namespace farmui {
bool RuntimeUI::Initialize(SpriteCommon* common) {
    if (!common) return false;
    for (auto& sprite : panels_) if (!sprite.Initialize(common, "Resources/ui/hud_panel_fill.png")) return false;
    for (auto& sprite : labels_) if (!sprite.Initialize(common, "Resources/ui/farm_runtime_menu.png")) return false;
    for (auto& sprite : digits_) if (!sprite.Initialize(common, "Resources/ui/farm_runtime_menu.png")) return false;
    for (auto& sprite : radarLines_) if (!sprite.Initialize(common, "Resources/ui/hud_panel_fill.png")) return false;
    if (!names_.Initialize(common, "Resources/ui/hud_panel_fill.png")) return false;
    ready_ = true;
    return true;
}
void RuntimeUI::PrepareNames(const View& view) {
    if (!ready_ || view.recordNames[0].empty() || cachedNames_ == view.recordNames) return;
    cachedNames_ = view.recordNames;
    namesReady_ = false;
    Gdiplus::GdiplusStartupInput startup{}; ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &startup, nullptr) != Gdiplus::Ok) return;
    {
        constexpr int width = 900, height = 112;
        DirectX::ScratchImage pixels;
        if (SUCCEEDED(pixels.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1))) {
            Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppARGB);
            Gdiplus::Graphics graphics(&bitmap);
            graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
            graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
            Gdiplus::Font font(L"Yu Gothic UI", 26, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush brush(Gdiplus::Color(255, 255, 234, 166));
            Gdiplus::StringFormat format;
            format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
            format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
            for (int i = 0; i < 2; ++i) {
                const std::string text = (i == 0 ? "現在の保存名：" : "選択中の保存名：") + view.recordNames[i];
                const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
                if (length <= 0 || length > 256) continue;
                std::wstring wide(length, L'\0');
                MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), wide.data(), length);
                graphics.DrawString(wide.c_str(), length, &font, Gdiplus::RectF(0, static_cast<float>(i * 56), width, 50), &format, &brush);
            }
            graphics.Flush(Gdiplus::FlushIntentionSync);
            Gdiplus::BitmapData data{}; Gdiplus::Rect rect(0, 0, width, height);
            if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) == Gdiplus::Ok) {
                const auto* destinationImage = pixels.GetImage(0, 0, 0);
                for (int y = 0; y < height; ++y) {
                    const auto* source = static_cast<const uint8_t*>(data.Scan0) + static_cast<std::ptrdiff_t>(y) * data.Stride;
                    auto* destination = destinationImage->pixels + y * destinationImage->rowPitch;
                    for (int x = 0; x < width; ++x) {
                        destination[x*4] = source[x*4+2]; destination[x*4+1] = source[x*4+1];
                        destination[x*4+2] = source[x*4]; destination[x*4+3] = source[x*4+3];
                    }
                }
                bitmap.UnlockBits(&data);
                const auto texture = TextureManager::GetInstance()->UpdateUiTexture("farm-record-names", pixels);
                if (texture.IsValid()) {
                    names_.SetTexture(texture); names_.SetTextureRect({0, 0}, {width, height});
                    names_.SetPosition({176, 426}); names_.SetSize({width, height});
                    names_.SetColor({1, 1, 1, 1}); namesReady_ = true;
                }
            }
        }
    }
    Gdiplus::GdiplusShutdown(token);
}

void RuntimeUI::Panel(Rect r, Vector4 color) {
    if (panelCount_ >= panels_.size()) return;
    auto& sprite = panels_[panelCount_++];
    sprite.SetPosition({r.x, r.y}); sprite.SetSize({r.width, r.height});
    sprite.SetColor(color); sprite.Update(); sprite.Draw();
}
void RuntimeUI::LabelQuad(Label label, Rect r, Vector4 color) {
    const auto index = static_cast<std::size_t>(label);
    if (index >= kLabels.size() || labelCount_ >= labels_.size()) return;
    const auto& uv = kLabels[index];
    const float scale = (std::min)(1.0f, (std::max)(0.0f, r.width - 20.0f) / uv.width);
    auto& sprite = labels_[labelCount_++];
    sprite.SetTextureRect({uv.x, uv.y}, {uv.width, uv.height});
    sprite.SetPosition({r.x + 10, r.y + (r.height - uv.height * scale) * 0.5f});
    sprite.SetSize({uv.width * scale, uv.height * scale});
    sprite.SetColor(color); sprite.Update(); sprite.Draw();
}
void RuntimeUI::ValueText(const std::string& value, Vector2 position, float right) {
    // Numeric/status identifiers only; Japanese labels use whole-line atlas regions.
    for (const unsigned char c : value) {
        if (digitCount_ >= digits_.size() || position.x + 24 > right) break;
        const int glyph = (c >= 32 && c <= 126 ? c : '?') - 32;
        auto& sprite = digits_[digitCount_++];
        sprite.SetTextureRect({static_cast<float>((glyph % 40) * 32), kAsciiY + (glyph / 40) * 40.0f}, {24, 36});
        sprite.SetSize({24, 36}); sprite.SetPosition(position);
        sprite.SetColor({1.0f, 0.87f, 0.50f, 1}); sprite.Update(); sprite.Draw();
        position.x += 14;
    }
}
void RuntimeUI::DrawRadar(const QualityRadar& radar) {
    std::size_t count = 0;
    const auto line = [&](Vector2 a, Vector2 b, float thickness, Vector4 color) {
        const float dx = b.x-a.x, dy = b.y-a.y;
        const float length = std::hypot(dx,dy);
        if (!std::isfinite(length) || length < 0.01f || count >= radarLines_.size()) return;
        auto& sprite = radarLines_[count++];
        sprite.SetAnchorPoint({0,0.5f}); sprite.SetPosition(a);
        sprite.SetRotation(std::atan2(dy,dx)); sprite.SetSize({length,thickness});
        sprite.SetColor(color); sprite.Update(); sprite.Draw();
    };
    const Vector4 grid{0.35f,0.43f,0.46f,1}, gold{1,0.81f,0.25f,1};
    for (int ring=1; ring<=4; ++ring) for (std::size_t i=0; i<4; ++i)
        line(QualityRadar::Point(i,ring*0.25f), QualityRadar::Point((i+1)%4,ring*0.25f), 1.5f, grid);
    for (std::size_t i=0; i<4; ++i) {
        line(QualityRadar::center, QualityRadar::Point(i,1), 1.5f, grid);
        const auto next = (i+1)%4;
        // Unknown values leave a gap instead of inventing a zero-valued measurement.
        if (radar.known[i] && radar.known[next])
            line(QualityRadar::Point(i,radar.values[i]), QualityRadar::Point(next,radar.values[next]), 4, gold);
        if (radar.known[i]) {
            const auto point = QualityRadar::Point(i,radar.values[i]);
            line({point.x-4,point.y}, {point.x+4,point.y}, 8, gold);
        }
    }
    constexpr std::array<Vector2,4> numbers{{{356,192},{490,322},{356,454},{220,322}}};
    for (std::size_t i=0; i<4; ++i) ValueText(std::to_string(i+1), numbers[i], numbers[i].x+28);
}
void RuntimeUI::Draw(const View& view, Vector2 pointer) {
    if (!ready_) return;
    panelCount_ = labelCount_ = digitCount_ = 0;
    if (view.modal) {
        Panel({0, 0, 1280, 720}, {0.01f, 0.015f, 0.02f, 0.75f});
        Panel({130, 32, 1020, 656}, {0.055f, 0.075f, 0.072f, 0.99f});
    }
    if (view.observation) {
        Panel(View::kObservationTop, {0.035f, 0.055f, 0.06f, 0.96f});
        Panel(View::kObservationBottom, {0.035f, 0.055f, 0.06f, 0.96f});
    }
    if (view.terrain) {
        Panel(View::kTerrainTop, {0.035f, 0.055f, 0.06f, 0.96f});
        Panel(View::kTerrainBottom, {0.035f, 0.055f, 0.06f, 0.96f});
    }
    if (view.fieldActions) Panel(View::kFieldActionsPanel, {0.035f, 0.055f, 0.06f, 0.96f});
    if (view.radar.visible) DrawRadar(view.radar);
    for (std::size_t i = 0; i < (std::min)(view.count, view.items.size()); ++i) {
        const auto& item = view.items[i];
        const bool button = item.request.action != Action::None;
        if (button) {
            Vector4 color = item.enabled ? Vector4{0.16f, 0.23f, 0.22f, 1} : Vector4{0.09f, 0.11f, 0.11f, 1};
            if (item.selected) color = {0.40f, 0.32f, 0.10f, 1};
            if (item.enabled && (item.focused || item.rect.Contains(pointer))) color = {0.25f, 0.40f, 0.36f, 1};
            Panel(item.rect, color);
        }
        Rect labelRect = item.rect;
        if (!item.value.empty()) labelRect.width = item.valueOffset - 10;
        LabelQuad(item.label, labelRect, item.enabled ? Vector4{0.96f, 0.98f, 0.96f, 1} : Vector4{0.42f, 0.46f, 0.45f, 1});
        if (!item.value.empty()) ValueText(item.value, {item.rect.x + item.valueOffset, item.rect.y + 1}, item.rect.x + item.rect.width);
    }
    if (!view.recordNames[0].empty() && namesReady_) { names_.Update(); names_.Draw(); }
}
}
