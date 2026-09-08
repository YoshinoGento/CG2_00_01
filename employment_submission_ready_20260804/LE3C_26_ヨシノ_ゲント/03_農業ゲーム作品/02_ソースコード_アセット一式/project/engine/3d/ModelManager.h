#pragma once
#include <map>
#include <string>
#include <memory>

// 前方宣言
class Model;
class DirectXCommon;
class SrvManager;

/**
 * ModelManagerクラス
 * 3Dモデル（.obj）の読み込みと管理を行います。
 */
class ModelManager {
public:
	ModelManager();
	~ModelManager();

	// 初期化
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	// モデルの読み込みと取得
	void LoadModel(const std::string& filename);
	Model* GetModel(const std::string& filename);

	// --- ゲッター ---
	// ★エラー修正ポイント：中身 { return ... } を消して「 ; 」だけにします
	DirectXCommon* GetDxCommon() const;
	SrvManager* GetSrvManager() const;

private:
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	// モデルを名前で管理
	std::map<std::string, std::unique_ptr<Model>> models_;
};