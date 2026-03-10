#pragma once
#include <map>
#include <string>
#include <memory>

// --- 前方宣言（ぜんぽうぜんげん） ---
// クラス名だけを教えることで、ビルド速度を上げ、エラーを防ぎます。
class Model;
class DirectXCommon;
class SrvManager;

/**
 * ModelManagerクラス
 * 3Dモデル（.obj）の読み込みと管理を専門に行うクラスです。
 */
class ModelManager {
public:
	// コンストラクタ
	ModelManager();

	// ★重要：デストラクタは宣言のみ。
	// 実体は Model.h を読み込んでいる ModelManager.cpp で定義します。
	~ModelManager();

	// コピー禁止（マネージャが複数作られるのを防ぐため）
	ModelManager(const ModelManager&) = delete;
	ModelManager& operator=(const ModelManager&) = delete;

	/**
	 * 初期化
	 * @param dxCommon DirectX基盤のポインタ
	 * @param srvManager SRV管理のポインタ
	 */
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	/**
	 * モデルの読み込み
	 * @param filename モデルのファイル名（例："plane.obj"）
	 */
	void LoadModel(const std::string& filename);

	/**
	 * モデルの取得
	 * @param filename 取得したいモデル名
	 * @return モデルへのポインタ
	 */
	Model* GetModel(const std::string& filename);

	// --- ゲッター ---
	DirectXCommon* GetDxCommon() const { return dxCommon_; }
	SrvManager* GetSrvManager() const { return srvManager_; }

private:
	// DirectX共通基盤へのポインタ
	DirectXCommon* dxCommon_ = nullptr;
	// SRV管理クラスへのポインタ
	SrvManager* srvManager_ = nullptr;

	// 読み込んだモデルを「名前」と「本体」のセットで管理するマップ
	// unique_ptr を使ってメモリ管理を自動化しています。
	std::map<std::string, std::unique_ptr<Model>> models_;
};