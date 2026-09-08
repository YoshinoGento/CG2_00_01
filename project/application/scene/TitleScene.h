#pragma once
#include "BaseScene.h"
#include "RankingPresentationSystem.h"
#include "2d/BitmapFont.h"
#include "2d/SpriteText.h"

#include <array>
#include <memory>

class Camera;
class Model;
class Object3d;
class Skybox;
class Sprite;

/**
 * TitleScene
 * ゲームの開始待機画面。
 */
class TitleScene : public BaseScene {
public:
	enum class Page { Title, Instructions, StageSelect, Ranking };
	explicit TitleScene(Page page = Page::Title) noexcept : page_(page) {}
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	void InitializeRankingDecorations();
	void UpdateRankingPresentation(float deltaSeconds);
	void RefreshTransitionPrompt(bool useGamepad);
	void RefreshStageSelectLines();
	void UpdateStageSelectVisuals(float deltaTime);
	void EmitStageSelectParticles(bool selectionBurst);
	void SetLine(std::size_t index, const std::string& text,
		const Vector2& position, float scale, const Vector4& color);

	Page page_ = Page::Title;
	BitmapFont font_;
	BitmapFont rankingStatusFont_;
	std::unique_ptr<Camera> titleCamera_;
	std::unique_ptr<Skybox> menuSkybox_;
	std::unique_ptr<Object3d> titleObject_;
	std::unique_ptr<Object3d> transitionKeyObject_;
	Model* keyboardTransitionModel_ = nullptr;
	Model* gamepadTransitionModel_ = nullptr;
	std::unique_ptr<Object3d> guideTitleObject_;
	std::unique_ptr<Object3d> operationGuideObject_;
	std::unique_ptr<Object3d> rankingTitleObject_;
	std::array<std::unique_ptr<Object3d>, 5> rankingScoreObjects_{};
	std::array<SpriteText, 5> rankingValueTexts_{};
	SpriteText rankingStatusText_{};
	std::unique_ptr<Sprite> rankingCurrentArrowSprite_;
	std::unique_ptr<Object3d> rankingPlayerObject_;
	std::array<std::unique_ptr<Object3d>,
		RankingPresentationFrame::kDecorationBallCount> rankingBallObjects_{};
	RankingPresentationSystem rankingPresentationSystem_{};
	std::array<SpriteText, 11> lines_{};
	std::size_t lineCount_ = 0;
	bool transitionPromptInitialized_ = false;
	bool transitionPromptUsesGamepad_ = false;
	bool transitionPromptModelVisible_ = false;
	bool rankingCurrentArrowVisible_ = false;
	bool rankingStatusReady_ = false;
	bool rankingStatusVisible_ = false;
	int stageSelection_ = 0;
	bool stageSelectStickUpWasPressed_ = false;
	bool stageSelectStickDownWasPressed_ = false;
	float stageSelectAnimationSeconds_ = 0.0f;
	float stageSelectParticleTimer_ = 0.0f;
	uint32_t stageSelectParticleSequence_ = 0;
	bool uiReady_ = false;
};
