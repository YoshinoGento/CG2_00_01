#include "AnimatedSprite.h"
#include "2d/SpriteCommon.h"
#include "base/Logger.h"

bool AnimatedSprite::Initialize(
    SpriteCommon* spriteCommon,
    const std::string& texturePath,
    const Vector2& frameSize,
    int columns,
    int frameCount,
    float frameDuration,
    bool loop) {

    if (spriteCommon == nullptr) {
        Logger::Log("AnimatedSprite::Initialize failed. spriteCommon is null.");
        return false;
    }
    if (texturePath.empty()) {
        Logger::Log("AnimatedSprite::Initialize failed. texturePath is empty.");
        return false;
    }
    if (frameSize.x <= 0.0f || frameSize.y <= 0.0f) {
        Logger::Log("AnimatedSprite::Initialize failed. frameSize must be positive.");
        return false;
    }
    if (columns <= 0) {
        Logger::Log("AnimatedSprite::Initialize failed. columns must be positive.");
        return false;
    }
    if (frameCount <= 0) {
        Logger::Log("AnimatedSprite::Initialize failed. frameCount must be positive.");
        return false;
    }
    if (frameDuration <= 0.0f) {
        Logger::Log("AnimatedSprite::Initialize failed. frameDuration must be positive.");
        return false;
    }

    spriteCommon_ = spriteCommon;
    frameSize_ = frameSize;
    displaySize_ = frameSize;
    columns_ = columns;
    frameCount_ = frameCount;
    currentFrame_ = 0;
    frameDuration_ = frameDuration;
    elapsedTime_ = 0.0f;
    isPlaying_ = false;
    loop_ = loop;

    const uint32_t textureHandle = spriteCommon_->LoadTexture(texturePath);
    sprite_ = std::make_unique<Sprite>();
    sprite_->Initialize(spriteCommon_, textureHandle);
    sprite_->SetSize(displaySize_);
    ApplyCurrentFrameRect();

    return true;
}

void AnimatedSprite::Play() {
    if (sprite_ == nullptr) {
        return;
    }

    isPlaying_ = true;
}

void AnimatedSprite::Stop() {
    isPlaying_ = false;
}

void AnimatedSprite::Reset() {
    currentFrame_ = 0;
    elapsedTime_ = 0.0f;
    isPlaying_ = false;
    ApplyCurrentFrameRect();
}

void AnimatedSprite::SetPosition(const Vector2& position) {
    if (sprite_ == nullptr) {
        return;
    }

    sprite_->SetPosition(position);
}

void AnimatedSprite::SetSize(const Vector2& size) {
    if (sprite_ == nullptr) {
        return;
    }

    displaySize_ = size;
    sprite_->SetSize(displaySize_);
}

void AnimatedSprite::SetScale(float scale) {
    if (sprite_ == nullptr) {
        return;
    }
    if (scale <= 0.0f) {
        Logger::Log("AnimatedSprite::SetScale ignored. scale must be positive.");
        return;
    }

    displaySize_ = { frameSize_.x * scale, frameSize_.y * scale };
    sprite_->SetSize(displaySize_);
}

void AnimatedSprite::SetColor(const Vector4& color) {
    if (sprite_ == nullptr) {
        return;
    }

    sprite_->SetColor(color);
}

void AnimatedSprite::SetLoop(bool loop) {
    loop_ = loop;
}

void AnimatedSprite::Update(float deltaTime) {
    if (sprite_ == nullptr) {
        return;
    }

    if (isPlaying_ && deltaTime > 0.0f) {
        elapsedTime_ += deltaTime;

        bool frameChanged = false;
        while (elapsedTime_ >= frameDuration_ && isPlaying_) {
            elapsedTime_ -= frameDuration_;
            AdvanceFrame();
            frameChanged = true;
        }

        if (frameChanged) {
            ApplyCurrentFrameRect();
        }
    }

    sprite_->Update();
}

void AnimatedSprite::Draw() {
    if (sprite_ == nullptr) {
        return;
    }

    sprite_->Draw();
}

void AnimatedSprite::ApplyCurrentFrameRect() {
    if (sprite_ == nullptr || columns_ <= 0) {
        return;
    }

    const Vector2 leftTop = {
        static_cast<float>(currentFrame_ % columns_) * frameSize_.x,
        static_cast<float>(currentFrame_ / columns_) * frameSize_.y,
    };
    sprite_->SetTextureRect(leftTop, frameSize_);
    sprite_->SetSize(displaySize_);
}

void AnimatedSprite::AdvanceFrame() {
    ++currentFrame_;
    if (currentFrame_ < frameCount_) {
        return;
    }

    if (loop_) {
        currentFrame_ = 0;
        return;
    }

    currentFrame_ = frameCount_ - 1;
    elapsedTime_ = 0.0f;
    isPlaying_ = false;
}
