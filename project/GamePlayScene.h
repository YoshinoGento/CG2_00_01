#pragma once
#include "BaseScene.h"
#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include "Matrix.h"

class Framework;
class Sprite;
class Object3d;
class Camera;

/**
 * GamePlaySceneクラス
 * デバッグ機能を備えたプレイシーンです。
 * スナップ機能を削除し、自由な移動が可能な状態に修正しました。
 */
class GamePlayScene : public BaseScene {
public:
	GamePlayScene();
	~GamePlayScene() override;

	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	void AddLog(const std::string& message);
	void HandleKeyboardMovement();

private:
	Framework* framework_ = nullptr;

	// --- 描画オブジェクト ---
	std::unique_ptr<Sprite> sprite_;
	std::unique_ptr<Object3d> object3d_;
	std::unique_ptr<Camera> camera_;

	// --- アセット管理 ---
	std::vector<std::string> textureNames_ = {
		"monsterBall.png", "uvChecker.png", "choju8_0008.png",
		"IMG_0264.jpg", "仏顔.jpg", "魚パン.png"
	};
	std::vector<uint32_t> textureHandles_;
	int currentSpriteTexIndex_ = 0;
	int currentObjTexIndex_ = 1;
	int currentParticleTexIndex_ = 2;

	// --- パラメータ ---
	Vector2 spritePos_ = { 640.0f, 360.0f };
	Vector3 objectPos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 objectRot_ = { 0.0f, 0.0f, 0.0f };
	int modelPriority_ = 0;

	// --- 編集設定 ---
	int selectedTarget_ = 1; // 1:Sprite, 2:Object3D, 3:Particle
	// ★スナップ関連の変数を削除しました

	// --- オーディオ ---
	uint32_t soundHandles_[2] = { 0, 0 };
	int currentBgmIndex_ = 0;
	bool isBgmLoop_ = true;

	// --- パーティクル ---
	float particleEmitPos_[3] = { 0.0f, 0.0f, 0.0f };
	int particleEmitCount_ = 15;

	// --- デバッグ ---
	std::vector<std::string> debugLogs_;
	float frameRates_[60] = {};
	int frameRateIndex_ = 0;
};