#pragma once
#include "BaseScene.h"
#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include "Matrix.h"
#include "Skybox.h"

class Framework;
class Sprite;
class Object3d;
class Camera;
class Model;

/**
 * GamePlaySceneクラス
 */
class GamePlayScene : public BaseScene {
	friend class Game;

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

	Vector3 cameraPos_ = { 0.0f, 5.0f, -15.0f };
	Vector3 cameraRot_ = { 0.3f, 0.0f, 0.0f };

	std::unique_ptr<Model> sphereModel_;
	std::unique_ptr<Object3d> sphereObj_;
	std::unique_ptr<Model> terrainModel_;
	std::unique_ptr<Object3d> terrainObj_;
	std::unique_ptr<Skybox> skybox_; // スカイボックス本体

	std::vector<uint32_t> textureHandles_;

	// --- 表示フラグ ---
	bool showTerrain_ = true;
	bool showSphere_ = true;
	bool showPlane_ = true;
	bool showSprite_ = true;
	bool showParticles_ = true;

	// --- エディタ操作用変数 ---
	Vector2 spritePos_ = { 640.0f, 360.0f };
	Vector3 objectPos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 objectRot_ = { 0.0f, 0.0f, 0.0f };

	float sphereRadius_ = 1.0f;
	Vector3 spherePos_ = { 0.0f, 0.0f, 10.0f };

	int selectedTarget_ = 1;
	int modelPriority_ = 0;
	int cullMode_ = 2;

	// 平行光源設定
	Vector3 lightDirection_ = { 0.0f, -1.0f, 1.0f };
	Vector3 lightColor_ = { 1.0f, 1.0f, 1.0f };
	float lightIntensity_ = 1.0f;

	// ★追加：スポットライト設定
	Vector3 spotLightColor_ = { 1.0f, 1.0f, 1.0f };
	Vector3 spotLightPos_ = { 0.0f, 4.0f, 10.0f };
	float spotLightIntensity_ = 2.0f;
	Vector3 spotLightDir_ = { 0.0f, -1.0f, 0.0f };
	float spotLightDistance_ = 15.0f;
	float spotLightDecay_ = 1.0f;
	float spotLightAngle_ = 0.5f; // ラジアン
	float spotLightFalloff_ = 0.1f; // ラジアン

	std::vector<std::string> debugLogs_;

};