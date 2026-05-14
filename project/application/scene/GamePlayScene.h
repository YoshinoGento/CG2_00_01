#pragma once
#include "BaseScene.h"
#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include "math/Matrix.h"
#include "3d/Skybox.h"

class Framework;
class Sprite;
class Object3d;
class Camera;
class Model;

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
	// Cylinderメッシュの再生成（パラメータ変更時に呼ぶ）
	void RebuildCylinder();

private:
	void AddLog(const std::string& message);
	void HandleKeyboardMovement();

	// エフェクト発生関数
	void EmitSpark(const Vector3& position);
	void EmitRingEffect(const Vector3& position);
	void EmitCylinderEffect(const Vector3& position);

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

	std::unique_ptr<Object3d> animObj_;

	// エフェクト用モデル
	std::unique_ptr<Model> ringModel_;
	std::unique_ptr<Model> cylinderModel_;

	std::unique_ptr<Skybox> skybox_;
	std::vector<uint32_t> textureHandles_;

	bool showTerrain_ = true;
	bool showSphere_ = true;
	bool showPlane_ = true;
	bool showSprite_ = true;
	bool showParticles_ = true;
	bool showAnimModel_ = true;

	Vector2 spritePos_ = { 640.0f, 360.0f };
	Vector3 objectPos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 objectRot_ = { 0.0f, 0.0f, 0.0f };
	Vector3 spherePos_ = { 0.0f, 0.0f, 10.0f };
	float sphereRadius_ = 1.0f;

	int selectedTarget_ = 4;
	int activeParticleType_ = 0; // これを Game.cpp と同期
	int cullMode_ = 2;
	int modelPriority_ = 0;

	// Cylinderパラメータ（ImGuiで操作可能）
	float cylTopRadius_ = 1.0f;       // 上面の半径
	float cylBottomRadius_ = 1.0f;    // 下面の半径
	float cylHeight_ = 2.0f;          // 高さ
	int cylSegments_ = 32;            // 円周方向の分割数
	int cylVertDivisions_ = 4;        // 高さ方向の分割数

	// ライト設定 (GamePlayScene.cpp で使用しているもの)
	Vector3 lightDirection_ = { 0.0f, -1.0f, 1.0f };
	Vector3 lightColor_ = { 1.0f, 1.0f, 1.0f };
	float lightIntensity_ = 1.0f;
	Vector3 spotLightColor_ = { 1.0f, 1.0f, 1.0f };
	Vector3 spotLightPos_ = { 0.0f, 4.0f, 10.0f };
	float spotLightIntensity_ = 2.0f;
	Vector3 spotLightDir_ = { 0.0f, -1.0f, 0.0f };
	float spotLightDistance_ = 15.0f;
	float spotLightDecay_ = 1.0f;
	float spotLightAngle_ = 0.5f;
	float spotLightFalloff_ = 0.1f;

	std::vector<std::string> debugLogs_;
};