#pragma once
#include "title/TitlePresentationSystem.h"
#include "2d/Sprite.h"
#include <memory>
class Input;
class WinApp;
namespace title {
class TitleView final {
public:
    bool Initialize(SpriteCommon* common);
    [[nodiscard]] bool IsStartRequested(const Input& input, const WinApp& window) const;
    void Draw(const Frame& frame);
private:
    SpriteCommon* common_ = nullptr;
    std::unique_ptr<Sprite> logo_, prompt_, fade_;
};
}
