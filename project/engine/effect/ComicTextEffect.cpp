#include "effect/ComicTextEffect.h"

#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "base/Logger.h"
#include "io/JsonFile.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace {
constexpr const char* kPresetDirectory = "Settings/effects/comic/";
constexpr float kVirtualWidth = 1280.0f;
constexpr float kVirtualHeight = 720.0f;
constexpr const char* kComicFontPath = "Resources/ui/font/comic_katakana_font.json";

float SmoothStep(float value) {
	value = std::clamp(value, 0.0f, 1.0f);
	return value * value * (3.0f - 2.0f * value);
}

bool ReadVector2(const nlohmann::json& json, const char* key, Vector2& value) {
	if (!json.contains(key) || !json.at(key).is_array() || json.at(key).size() != 2) {
		return false;
	}
	value = { json.at(key).at(0).get<float>(), json.at(key).at(1).get<float>() };
	return std::isfinite(value.x) && std::isfinite(value.y);
}

bool ReadVector4(const nlohmann::json& json, const char* key, Vector4& value) {
	if (!json.contains(key) || !json.at(key).is_array() || json.at(key).size() != 4) {
		return false;
	}
	value = { json.at(key).at(0).get<float>(), json.at(key).at(1).get<float>(),
		json.at(key).at(2).get<float>(), json.at(key).at(3).get<float>() };
	return std::isfinite(value.x) && std::isfinite(value.y) &&
		std::isfinite(value.z) && std::isfinite(value.w);
}
}

bool ComicTextEffectSystem::Initialize(SpriteCommon* spriteCommon) {
	spriteCommon_ = spriteCommon;
	effects_.clear();
	comicFont_ = std::make_unique<BitmapFont>();
	return spriteCommon_ != nullptr && comicFont_->InitializeFromJson(spriteCommon_, kComicFontPath);
}

void ComicTextEffectSystem::Clear() {
	effects_.clear();
}

bool ComicTextEffectSystem::Play(const ComicTextEffectPreset& preset, const Vector3& worldPosition) {
	if (!spriteCommon_ || preset.texturePath.empty() || preset.duration <= 0.0f) {
		return false;
	}
	ActiveEffect effect{};
	effect.preset = preset;
	effect.worldPosition = worldPosition;
	if (preset.useEditableText && !preset.text.empty() && comicFont_) {
		effect.extrusionText = std::make_unique<SpriteText>();
		effect.mainText = std::make_unique<SpriteText>();
		effect.extrusionText->Initialize(spriteCommon_, comicFont_.get());
		effect.mainText->Initialize(spriteCommon_, comicFont_.get());
		effect.extrusionText->SetText(preset.text);
		effect.mainText->SetText(preset.text);
		effect.extrusionText->SetCharacterSpacing(preset.characterSpacing);
		effect.mainText->SetCharacterSpacing(preset.characterSpacing);
		effects_.push_back(std::move(effect));
		return true;
	}
	effect.sprite = std::make_unique<Sprite>();
	if (!effect.sprite->Initialize(spriteCommon_, preset.texturePath)) {
		return false;
	}
	effect.sprite->SetAnchorPoint({ 0.5f, 0.5f });
	effects_.push_back(std::move(effect));
	return true;
}

bool ComicTextEffectSystem::Play(const std::string& presetName, const Vector3& worldPosition) {
	ComicTextEffectPreset preset{};
	return LoadPreset(presetName, preset) && Play(preset, worldPosition);
}

void ComicTextEffectSystem::Update(float deltaTime, const Matrix4x4& viewProjection) {
	deltaTime = std::clamp(deltaTime, 0.0f, 0.1f);
	for (ActiveEffect& effect : effects_) {
		effect.elapsed += deltaTime;
		const float duration = (std::max)(effect.preset.duration, 0.01f);
		const float progress = std::clamp(effect.elapsed / duration, 0.0f, 1.0f);
		const Vector3 ndc = MatrixMath::Transform(effect.worldPosition, viewProjection);
		const float clipW = effect.worldPosition.x * viewProjection.m[0][3] +
			effect.worldPosition.y * viewProjection.m[1][3] +
			effect.worldPosition.z * viewProjection.m[2][3] + viewProjection.m[3][3];
		effect.visible = clipW > 0.0f && ndc.z >= 0.0f && ndc.z <= 1.0f;

		const float popEnd = std::clamp(effect.preset.popFraction, 0.01f, 0.95f);
		float scale = effect.preset.endScale;
		if (progress < popEnd) {
			const float t = SmoothStep(progress / popEnd);
			scale = effect.preset.startScale + (effect.preset.peakScale - effect.preset.startScale) * t;
		} else {
			const float t = SmoothStep((progress - popEnd) / (1.0f - popEnd));
			scale = effect.preset.peakScale + (effect.preset.endScale - effect.preset.peakScale) * t;
		}

		const float shakeFade = 1.0f - progress;
		const float shake = std::sin(effect.elapsed * effect.preset.shakeFrequency) *
			effect.preset.shakeAmplitude * shakeFade;
		Vector2 position = {
			(ndc.x + 1.0f) * 0.5f * kVirtualWidth,
			(1.0f - ndc.y) * 0.5f * kVirtualHeight
		};
		position += effect.preset.screenOffset;
		position += effect.preset.drift * progress;
		position.x += shake;

		const float fadeStart = 1.0f - std::clamp(effect.preset.fadeFraction, 0.01f, 1.0f);
		const float alpha = progress <= fadeStart ? 1.0f :
			1.0f - SmoothStep((progress - fadeStart) / (std::max)(1.0f - fadeStart, 0.01f));
		if (effect.mainText && effect.extrusionText) {
			const float renderedScale = (std::max)(effect.preset.textScale * scale, 0.01f);
			std::size_t glyphCount = 0;
			for (unsigned char ch : effect.preset.text) {
				if ((ch & 0xC0) != 0x80) { ++glyphCount; }
			}
			const float width = static_cast<float>(glyphCount) * 96.0f * renderedScale +
				static_cast<float>(glyphCount > 0 ? glyphCount - 1 : 0) * effect.preset.characterSpacing;
			const Vector2 topLeft = { position.x - width * 0.5f, position.y - 48.0f * renderedScale };
			effect.extrusionText->SetPosition(topLeft + effect.preset.extrusionOffset * scale);
			effect.mainText->SetPosition(topLeft);
			effect.extrusionText->SetScale(renderedScale);
			effect.mainText->SetScale(renderedScale);
			Vector4 extrusionColor = effect.preset.extrusionColor;
			Vector4 textColor = effect.preset.textColor;
			extrusionColor.w *= alpha;
			textColor.w *= alpha;
			effect.extrusionText->SetColor(extrusionColor);
			effect.mainText->SetColor(textColor);
			effect.extrusionText->Update();
			effect.mainText->Update();
		} else if (effect.sprite) {
			effect.sprite->SetPosition(position);
			effect.sprite->SetSize(effect.preset.size * (std::max)(scale, 0.0f));
			effect.sprite->SetRotation(effect.preset.rotation);
			Vector4 color = effect.preset.color;
			color.w *= alpha;
			effect.sprite->SetColor(color);
			effect.sprite->Update();
		}
	}
	effects_.erase(std::remove_if(effects_.begin(), effects_.end(), [](const ActiveEffect& effect) {
		return effect.elapsed >= effect.preset.duration;
	}), effects_.end());
}

void ComicTextEffectSystem::Draw() {
	if (!spriteCommon_ || effects_.empty()) {
		return;
	}
	spriteCommon_->PreDraw();
	for (ActiveEffect& effect : effects_) {
		if (!effect.visible) { continue; }
		if (effect.extrusionText && effect.mainText) {
			effect.extrusionText->Draw();
			effect.mainText->Draw();
		} else if (effect.sprite) {
			effect.sprite->Draw();
		}
	}
}

bool ComicTextEffectSystem::IsSafePresetName(const std::string& presetName) {
	if (presetName.empty()) {
		return false;
	}
	for (const unsigned char ch : presetName) {
		if (!(std::isalnum(ch) || ch == '_' || ch == '-')) {
			return false;
		}
	}
	return true;
}

std::string ComicTextEffectSystem::BuildPresetPath(const std::string& presetName) {
	return std::string(kPresetDirectory) + presetName + ".json";
}

bool ComicTextEffectSystem::SavePreset(const std::string& presetName, const ComicTextEffectPreset& preset) {
	if (!IsSafePresetName(presetName)) {
		return false;
	}
	nlohmann::json json;
	json["type"] = "comicText";
	json["text"] = preset.text;
	json["useEditableText"] = preset.useEditableText;
	json["texturePath"] = preset.texturePath;
	json["textScale"] = preset.textScale;
	json["characterSpacing"] = preset.characterSpacing;
	json["extrusionOffset"] = { preset.extrusionOffset.x, preset.extrusionOffset.y };
	json["textColor"] = { preset.textColor.x, preset.textColor.y, preset.textColor.z, preset.textColor.w };
	json["extrusionColor"] = { preset.extrusionColor.x, preset.extrusionColor.y, preset.extrusionColor.z, preset.extrusionColor.w };
	json["size"] = { preset.size.x, preset.size.y };
	json["screenOffset"] = { preset.screenOffset.x, preset.screenOffset.y };
	json["drift"] = { preset.drift.x, preset.drift.y };
	json["color"] = { preset.color.x, preset.color.y, preset.color.z, preset.color.w };
	json["duration"] = preset.duration;
	json["startScale"] = preset.startScale;
	json["peakScale"] = preset.peakScale;
	json["endScale"] = preset.endScale;
	json["popFraction"] = preset.popFraction;
	json["fadeFraction"] = preset.fadeFraction;
	json["rotation"] = preset.rotation;
	json["shakeAmplitude"] = preset.shakeAmplitude;
	json["shakeFrequency"] = preset.shakeFrequency;
	return JsonFile::Save(BuildPresetPath(presetName), json);
}

bool ComicTextEffectSystem::LoadPreset(const std::string& presetName, ComicTextEffectPreset& preset) {
	if (!IsSafePresetName(presetName)) {
		return false;
	}
	nlohmann::json json;
	if (!JsonFile::Load(BuildPresetPath(presetName), json)) {
		return false;
	}
	try {
		ComicTextEffectPreset loaded{};
		loaded.text = json.value("text", loaded.text);
		loaded.useEditableText = json.value("useEditableText", loaded.useEditableText);
		loaded.texturePath = json.value("texturePath", loaded.texturePath);
		loaded.textScale = std::clamp(json.value("textScale", loaded.textScale), 0.1f, 10.0f);
		loaded.characterSpacing = std::clamp(json.value("characterSpacing", loaded.characterSpacing), -96.0f, 200.0f);
		ReadVector2(json, "extrusionOffset", loaded.extrusionOffset);
		ReadVector4(json, "textColor", loaded.textColor);
		ReadVector4(json, "extrusionColor", loaded.extrusionColor);
		ReadVector2(json, "size", loaded.size);
		ReadVector2(json, "screenOffset", loaded.screenOffset);
		ReadVector2(json, "drift", loaded.drift);
		ReadVector4(json, "color", loaded.color);
		loaded.duration = std::clamp(json.value("duration", loaded.duration), 0.05f, 10.0f);
		loaded.startScale = std::clamp(json.value("startScale", loaded.startScale), 0.0f, 10.0f);
		loaded.peakScale = std::clamp(json.value("peakScale", loaded.peakScale), 0.0f, 10.0f);
		loaded.endScale = std::clamp(json.value("endScale", loaded.endScale), 0.0f, 10.0f);
		loaded.popFraction = std::clamp(json.value("popFraction", loaded.popFraction), 0.01f, 0.95f);
		loaded.fadeFraction = std::clamp(json.value("fadeFraction", loaded.fadeFraction), 0.01f, 1.0f);
		loaded.rotation = json.value("rotation", loaded.rotation);
		loaded.shakeAmplitude = std::clamp(json.value("shakeAmplitude", loaded.shakeAmplitude), 0.0f, 100.0f);
		loaded.shakeFrequency = std::clamp(json.value("shakeFrequency", loaded.shakeFrequency), 0.0f, 200.0f);
		if (loaded.texturePath.empty()) {
			return false;
		}
		preset = loaded;
		return true;
	} catch (const nlohmann::json::exception& exception) {
		Logger::Log("ComicTextEffectSystem failed to parse preset: " + std::string(exception.what()));
		return false;
	}
}
