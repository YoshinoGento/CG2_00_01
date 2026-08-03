#pragma once
#include "2d/Sprite.h"
#include "math/Struct.h"
#include <memory>
#include <string>

class AnimatedSprite {
public:
    bool Initialize(
        SpriteCommon* spriteCommon,
        const std::string& texturePath,
        const Vector2& frameSize,
        int columns,
        int frameCount,
        float frameDuration,
        bool loop = true);

    void Play();
    void Stop();
    void Reset();

    void SetPosition(const Vector2& position);
    void SetSize(const Vector2& size);
    void SetScale(float scale);
    void SetColor(const Vector4& color);
    void SetLoop(bool loop);

    void Update(float deltaTime);
    void Draw();

    int GetCurrentFrame() const { return currentFrame_; }
    bool IsPlaying() const { return isPlaying_; }

private:
    void ApplyCurrentFrameRect();
    void AdvanceFrame();

private:
    SpriteCommon* spriteCommon_ = nullptr;
    std::unique_ptr<Sprite> sprite_;

    Vector2 frameSize_{};
    Vector2 displaySize_{};
    int columns_ = 0;
    int frameCount_ = 0;
    int currentFrame_ = 0;

    float frameDuration_ = 0.0f;
    float elapsedTime_ = 0.0f;

    bool isPlaying_ = false;
    bool loop_ = true;
};
