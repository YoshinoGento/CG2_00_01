#include "TitleScene.h"

#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "base/Framework.h"
#include "io/Input.h"
#include "math/Matrix.h"
#include "SceneManager.h"

#include <memory>
#include <string>

namespace {
constexpr float kVirtualWidth = 1280.0f;
constexpr float kVirtualHeight = 720.0f;
constexpr Vector2 kScreenCenter = { kVirtualWidth * 0.5f, kVirtualHeight * 0.5f };
constexpr Vector2 kTitleLogoPosition = { kVirtualWidth * 0.5f, 250.0f };
constexpr Vector2 kPressSpacePosition = { kVirtualWidth * 0.5f, 460.0f };

std::unique_ptr<Sprite> CreateCenteredSprite(
	SpriteCommon* spriteCommon,
	const std::string& texturePath,
	const Vector2& position,
	float height,
	const Vector4& color = { 1.0f, 1.0f, 1.0f, 1.0f }) {
	if (!spriteCommon || height <= 0.0f) {
		return nullptr;
	}

	const uint32_t textureHandle = spriteCommon->LoadTexture(texturePath);
	const D3D12_RESOURCE_DESC textureDesc = spriteCommon->GetTextureResourceDesc(textureHandle);
	const float textureWidth = static_cast<float>(textureDesc.Width);
	const float textureHeight = static_cast<float>(textureDesc.Height);
	const float aspect = textureHeight > 0.0f ? textureWidth / textureHeight : 1.0f;

	auto sprite = std::make_unique<Sprite>();
	sprite->Initialize(spriteCommon, textureHandle);
	sprite->SetAnchorPoint({ 0.5f, 0.5f });
	sprite->SetPosition(position);
	sprite->SetSize({ height * aspect, height });
	sprite->SetColor(color);
	return sprite;
}
}

void TitleScene::Initialize() {
	spriteCommon_ = Framework::GetInstance()->GetSpriteCommon();
	if (!spriteCommon_) {
		return;
	}

	backgroundSprite_ = CreateCenteredSprite(
		spriteCommon_,
		"Resources/human/white.png",
		kScreenCenter,
		kVirtualHeight,
		{ 0.015f, 0.035f, 0.060f, 1.0f });
	if (backgroundSprite_) {
		backgroundSprite_->SetSize({ kVirtualWidth, kVirtualHeight });
	}

	titleLogoSprite_ = CreateCenteredSprite(
		spriteCommon_,
		"Resources/generated/text/title_logo.png",
		kTitleLogoPosition,
		130.0f);

	pressSpaceSprite_ = CreateCenteredSprite(
		spriteCommon_,
		"Resources/generated/text/press_space.png",
		kPressSpacePosition,
		58.0f);
}

void TitleScene::Finalize() {
	pressSpaceSprite_.reset();
	titleLogoSprite_.reset();
	backgroundSprite_.reset();
	spriteCommon_ = nullptr;
}

void TitleScene::Update() {
	Input* input = Framework::GetInstance()->GetInput();
	if (input && input->TriggerKey(DIK_SPACE)) {
		if (sceneManager_) {
			sceneManager_->ChangeScene("GAMEPLAY");
		} else {
			SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
		}
	}
}

void TitleScene::Draw() {
	if (!spriteCommon_) {
		return;
	}

void TitleScene::Draw() {}
