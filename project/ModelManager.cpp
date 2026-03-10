#include "ModelManager.h"
#include "Model.h"       // ★重要: unique_ptrがModelを削除するために「中身」が必要
#include "DirectXCommon.h"
#include "SrvManager.h"
#include <cassert>

/**
 * コンストラクタ
 * ヘッダーで名前だけ登録（前方宣言）したクラスを扱うため、
 * 実体を知っているこの .cpp ファイルでデフォルト定義を行います。
 */
ModelManager::ModelManager() = default;

/**
 * デストラクタ
 * ここで Model.h が読み込まれているため、コンパイラは Model のサイズを知っており、
 * std::unique_ptr<Model> を安全に破棄（delete）することができます。
 */
ModelManager::~ModelManager() = default;

/**
 * 初期化
 * エンジンから DirectX 共通基盤と SRV 管理クラスを受け取ります。
 */
void ModelManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    assert(dxCommon);
    assert(srvManager);
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
}

/**
 * モデルの読み込み
 * 同じ名前のモデルが既に読み込まれている場合はスキップします。
 */
void ModelManager::LoadModel(const std::string& filename) {
    // 既に読み込み済みなら何もしない (C++20以降は contains が使えます)
    if (models_.find(filename) != models_.end()) {
        return;
    }

    // モデルの生成と初期化
    std::unique_ptr<Model> model = std::make_unique<Model>();

    // モデルに自分自身（this）を渡し、Resources フォルダから読み込む
    // ※Model側の Initialize の引数に合わせて調整してください
    model->Initialize(this, "Resources", filename);

    // マップに登録して管理
    models_[filename] = std::move(model);
}

/**
 * モデルの取得
 * 読み込まれているモデルの生ポインタを返します。
 */
Model* ModelManager::GetModel(const std::string& filename) {
    if (models_.find(filename) != models_.end()) {
        return models_[filename].get();
    }
    return nullptr;
}