#include "FloatingTextSystem.h"

#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace {
constexpr float kEpsilon = 0.0001f;
constexpr float kRewardPopupDuration = 1.25f;
constexpr Vector3 kRewardPopupVelocity = { 0.0f, 0.95f, 0.0f };
}

void FloatingTextSystem::Initialize(SpriteCommon* spriteCommon, const std::string& texturePath, uint32_t poolSize) {
	spriteCommon_ = spriteCommon;
	ready_ = false;
	floatingTexts_.clear();
	textures_.clear();

	if (!spriteCommon_ || poolSize == 0 || !std::filesystem::exists(texturePath)) {
		return;
	}

	if (!RegisterTexture("reward_120g", texturePath)) {
		return;
	}

	const TextureEntry& rewardTexture = textures_.at("reward_120g");
	textureHandle_ = rewardTexture.handle;
	textureBaseSize_ = rewardTexture.baseSize;

	floatingTexts_.resize(poolSize);
	for (FloatingText& text : floatingTexts_) {
		text.sprite = std::make_unique<Sprite>();
		text.sprite->Initialize(spriteCommon_, textureHandle_);
		text.sprite->SetAnchorPoint({ 0.5f, 0.5f });
		text.textureHandle = textureHandle_;
		text.baseSize = textureBaseSize_;
	}

	ready_ = true;
}

bool FloatingTextSystem::RegisterTexture(const std::string& id, const std::string& texturePath) {
	if (!spriteCommon_ || id.empty() || !std::filesystem::exists(texturePath)) {
		return false;
	}

	const uint32_t handle = spriteCommon_->LoadTexture(texturePath);
	const D3D12_RESOURCE_DESC textureDesc = spriteCommon_->GetTextureResourceDesc(handle);
	if (textureDesc.Width == 0 || textureDesc.Height == 0) {
		return false;
	}

	TextureEntry entry{};
	entry.handle = handle;
	entry.baseSize = {
		static_cast<float>(textureDesc.Width),
		static_cast<float>(textureDesc.Height),
	};
	textures_[id] = entry;
	return true;
}

void FloatingTextSystem::Update(float deltaTime) {
	if (!ready_) {
		return;
	}

	const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 0.25f);
	for (FloatingText& text : floatingTexts_) {
		if (!text.active) {
			continue;
		}

		text.worldPosition += text.velocity * safeDeltaTime;
		text.timer += safeDeltaTime;

		const float normalizedTime = std::clamp(text.timer / (std::max)(text.duration, kEpsilon), 0.0f, 1.0f);
		text.alpha = 1.0f - normalizedTime;
		text.scale = 1.0f + 0.25f * normalizedTime;

		if (text.timer >= text.duration) {
			text.active = false;
		}
	}
}

void FloatingTextSystem::Draw(
	const Vector2& viewportTopLeft,
	const Vector2& viewportSize,
	const Matrix4x4* viewProjection) {
	if (!ready_ || !spriteCommon_ || !viewProjection || viewportSize.x <= 1.0f || viewportSize.y <= 1.0f) {
		return;
	}

	bool hasVisibleText = false;
	for (const FloatingText& text : floatingTexts_) {
		if (!text.active) {
			continue;
		}

		Vector2 projectedPosition{};
		if (ProjectWorldToViewport(text.worldPosition, *viewProjection, viewportTopLeft, viewportSize, projectedPosition)) {
			hasVisibleText = true;
			break;
		}
	}
	if (!hasVisibleText) {
		return;
	}

	spriteCommon_->PreDraw();
	for (FloatingText& text : floatingTexts_) {
		if (!text.active || !text.sprite) {
			continue;
		}

		Vector2 projectedPosition{};
		if (!ProjectWorldToViewport(text.worldPosition, *viewProjection, viewportTopLeft, viewportSize, projectedPosition)) {
			continue;
		}

		text.sprite->SetPosition(projectedPosition);
		text.sprite->SetSize(text.baseSize * text.scale);
		text.sprite->SetColor({
			text.color.x,
			text.color.y,
			text.color.z,
			text.color.w * std::clamp(text.alpha, 0.0f, 1.0f)
			});
		text.sprite->Update();
		text.sprite->Draw();
	}
}

void FloatingTextSystem::PlayRewardPopup(const Vector3& worldPosition, int32_t price, float sizeScale, float duration) {
	(void)price;
	const float safeSizeScale = (std::max)(sizeScale, 0.1f);
	PlayFloatingText(
		worldPosition,
		"reward_120g",
		{ textureBaseSize_.x * safeSizeScale, textureBaseSize_.y * safeSizeScale },
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		(std::max)(duration, kRewardPopupDuration),
		kRewardPopupVelocity);
}

void FloatingTextSystem::PlayFloatingText(
	const Vector3& worldPosition,
	const std::string& textureId,
	const Vector2& size,
	const Vector4& color,
	float duration,
	const Vector3& velocity) {
	if (!ready_) {
		return;
	}
	const auto textureIt = textures_.find(textureId);
	if (textureIt == textures_.end()) {
		return;
	}

	FloatingText* text = AllocateFloatingText();
	if (!text || !text->sprite) {
		return;
	}

	text->active = true;
	text->worldPosition = ComputeStackedWorldPosition(worldPosition);
	text->velocity = velocity;
	text->timer = 0.0f;
	text->duration = (std::max)(duration, kEpsilon);
	text->alpha = 1.0f;
	text->scale = 1.0f;
	text->baseSize = size.x > 0.0f && size.y > 0.0f ? size : textureIt->second.baseSize;
	text->color = color;
	text->textureHandle = textureIt->second.handle;
	text->sprite->SetTexture(text->textureHandle);
	text->sprite->SetAnchorPoint({ 0.5f, 0.5f });
}

Vector3 FloatingTextSystem::ComputeStackedWorldPosition(const Vector3& worldPosition) const {
	int nearbyCount = 0;
	for (const FloatingText& text : floatingTexts_) {
		if (!text.active) {
			continue;
		}

		const float dx = text.worldPosition.x - worldPosition.x;
		const float dy = text.worldPosition.y - worldPosition.y;
		const float dz = text.worldPosition.z - worldPosition.z;
		const float horizontalDistanceSq = dx * dx + dz * dz;
		if (horizontalDistanceSq <= 0.42f * 0.42f && std::abs(dy) <= 1.15f) {
			++nearbyCount;
		}
	}

	const int stackIndex = nearbyCount % 5;
	switch (stackIndex) {
	case 1:
		return worldPosition + Vector3{ 0.24f, 0.24f, 0.04f };
	case 2:
		return worldPosition + Vector3{ -0.24f, 0.48f, -0.04f };
	case 3:
		return worldPosition + Vector3{ 0.0f, 0.72f, 0.16f };
	case 4:
		return worldPosition + Vector3{ 0.18f, 0.96f, -0.16f };
	default:
		return worldPosition;
	}
}

FloatingTextSystem::FloatingText* FloatingTextSystem::AllocateFloatingText() {
	for (FloatingText& text : floatingTexts_) {
		if (!text.active) {
			return &text;
		}
	}

	return &*std::max_element(
		floatingTexts_.begin(),
		floatingTexts_.end(),
		[](const FloatingText& lhs, const FloatingText& rhs) {
			return lhs.timer < rhs.timer;
		});
}

bool FloatingTextSystem::ProjectWorldToViewport(
	const Vector3& worldPosition,
	const Matrix4x4& viewProjection,
	const Vector2& viewportTopLeft,
	const Vector2& viewportSize,
	Vector2& outScreenPosition) {
	if (viewportSize.x <= 1.0f || viewportSize.y <= 1.0f) {
		return false;
	}
	if (!std::isfinite(worldPosition.x) || !std::isfinite(worldPosition.y) || !std::isfinite(worldPosition.z)) {
		return false;
	}

	const float clipX =
		worldPosition.x * viewProjection.m[0][0] +
		worldPosition.y * viewProjection.m[1][0] +
		worldPosition.z * viewProjection.m[2][0] +
		viewProjection.m[3][0];
	const float clipY =
		worldPosition.x * viewProjection.m[0][1] +
		worldPosition.y * viewProjection.m[1][1] +
		worldPosition.z * viewProjection.m[2][1] +
		viewProjection.m[3][1];
	const float clipZ =
		worldPosition.x * viewProjection.m[0][2] +
		worldPosition.y * viewProjection.m[1][2] +
		worldPosition.z * viewProjection.m[2][2] +
		viewProjection.m[3][2];
	const float clipW =
		worldPosition.x * viewProjection.m[0][3] +
		worldPosition.y * viewProjection.m[1][3] +
		worldPosition.z * viewProjection.m[2][3] +
		viewProjection.m[3][3];

	if (!std::isfinite(clipX) || !std::isfinite(clipY) || !std::isfinite(clipZ) || !std::isfinite(clipW)) {
		return false;
	}
	if (clipW <= kEpsilon) {
		return false;
	}

	const float ndcX = clipX / clipW;
	const float ndcY = clipY / clipW;
	const float ndcZ = clipZ / clipW;
	if (!std::isfinite(ndcX) || !std::isfinite(ndcY) || !std::isfinite(ndcZ)) {
		return false;
	}
	if (ndcX < -1.0f || ndcX > 1.0f || ndcY < -1.0f || ndcY > 1.0f || ndcZ < 0.0f || ndcZ > 1.0f) {
		return false;
	}

	outScreenPosition = {
		viewportTopLeft.x + (ndcX * 0.5f + 0.5f) * viewportSize.x,
		viewportTopLeft.y + (-ndcY * 0.5f + 0.5f) * viewportSize.y,
	};
	return std::isfinite(outScreenPosition.x) && std::isfinite(outScreenPosition.y);
}
