#pragma once
#include "Framework.h"
#include "Matrix.h" // Vector2等を使うため

// --- 前方宣言（ぜんぽうぜんげん） ---
// ヘッダーファイル内では「こういう名前のクラスがある」と伝えるだけに留めます。
// これによりビルド速度が上がり、エラーの主な原因である「循環参照」も防げます。
class Sprite;
class Object3d;
class Camera;
class Model;

/**
 * Gameクラス
 * Framework（骨組み）を継承して、このゲーム固有の処理を書く「アプリ層」のメインクラスです。
 */
class Game : public Framework {
public:
	// ★重要：コンストラクタとデストラクタは宣言のみ。実体は .cpp に書きます。
	// これが Camera や Model の「認識できない型」エラーを消す最大のポイントです。
	Game();
	~Game() override;

	// 親クラス Framework の仮想関数を上書き（override）して、中身を作ります。
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	// --- このゲーム固有のリソース ---
	// スマートポインタ（unique_ptr）で管理し、メモリ漏れを自動で防ぎます。
	std::unique_ptr<Sprite> sprite_;
	std::unique_ptr<Object3d> object3d_;
	std::unique_ptr<Camera> camera_;

	// --- 音楽管理用 ---
	// 配列を {0} で初期化することで C26495 の警告を防ぎます。
	uint32_t bgmHandles_[2] = { 0, 0 };
	int currentBgmIndex_ = 0; // 0: WAV, 1: MP3
	bool isBgmLoop_ = true;

	// --- デバッグ操作用パラメータ ---
	Vector2 spritePos_ = { 640.0f, 360.0f }; // スプライトの初期位置
	bool spriteOnTopVsParticle_ = true;      // スプライトをパーティクルより上に描くか
	int modelPriority_ = 0;                  // 0: Spriteが前, 1: Modelが前
};