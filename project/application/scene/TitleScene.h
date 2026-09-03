#pragma once
#include "BaseScene.h"
#include "2d/BitmapFont.h"
#include "2d/SpriteText.h"

#include <array>
#include <memory>

/**
 * TitleScene
 * ゲームの開始待機画面。
 */
class TitleScene : public BaseScene {
public:
	enum class Page { Title, Instructions, Ranking };
	explicit TitleScene(Page page = Page::Title) noexcept : page_(page) {}
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	void SetLine(std::size_t index, const std::string& text,
		const Vector2& position, float scale, const Vector4& color);

	Page page_ = Page::Title;
	BitmapFont font_;
	std::unique_ptr<Sprite> background_;
	std::array<SpriteText, 9> lines_{};
	std::size_t lineCount_ = 0;
	bool uiReady_ = false;
};
