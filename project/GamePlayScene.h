#pragma once
#include "BaseScene.h"
#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include "Matrix.h"

// 前方宣言
class Framework;
class Sprite;
class Object3d;
class Camera;
class Model;

class GamePlayScene : public BaseScene {
	friend class Game; // Gameクラスからアクセスを許可

public:
	GamePlayScene();
	~GamePlayScene() override;

	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

	void CreateSphere(float radius);

private:
	void AddLog(const std::string& message);
	void HandleKeyboardMovement();

private:
	Framework* framework_ = nullptr;

	std::unique_ptr<Sprite> sprite_;
	std::unique_ptr<Object3d> object3d_;
	std::unique_ptr<Camera> camera_;

	std::unique_ptr<Model> sphereModel_;
	std::unique_ptr<Object3d> sphereObj_;

	std::vector<std::string> textureNames_ = {
		"monsterBall.png", "uvChecker.png", "choju8_0008.png",
		"IMG_0264.jpg", "仏顔.jpg", "魚パン.png"
	};
	std::vector<uint32_t> textureHandles_;

	// --- エディタで操作する変数 ---
	Vector2 spritePos_ = { 640.0f, 360.0f };
	Vector3 objectPos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 objectRot_ = { 0.0f, 0.0f, 0.0f }; // ★追加：回転

	float sphereRadius_ = 1.0f;
	Vector3 spherePos_ = { 0.0f, 0.0f, 10.0f };

	int selectedTarget_ = 1;
	int modelPriority_ = 0; // ★追加：描画順序（0:3D->2D, 1:2D->3D）

	uint32_t soundHandles_[2] = { 0, 0 };
	int currentBgmIndex_ = 0;
	bool isBgmLoop_ = true;

	float particleEmitPos_[3] = { 0.0f, 0.0f, 0.0f };
	int particleEmitCount_ = 15;

	std::vector<std::string> debugLogs_;
};