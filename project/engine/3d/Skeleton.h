#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include "math/Struct.h"
#include "math/Matrix.h"
#include "3d/Model.h"
#include "3d/Animation.h"

// 【解説】
// 従来はObject3d内で「モデルのルートノード」の行列だけを計算（Rigid Animation）していましたが、
// 今後は「Skeleton（骨格）」を構築し、複数の骨（Joint）が親子関係を持って連動するようにします。

struct Joint {
	QuaternionTransform transform; // Local空間でのTransform情報
	Matrix4x4 localMatrix;         // Transformから計算されるLocal行列
	Matrix4x4 skeletonSpaceMatrix; // Skeleton空間（モデルの原点基準）での変換行列
	std::string name;              // Jointの名前
	std::vector<int32_t> children; // 子JointのIndexリスト
	int32_t index;                 // 自身のIndex
	std::optional<int32_t> parent; // 親JointのIndex。親がいなければnullopt
};

struct Skeleton {
	int32_t root;                            // RootJointのIndex
	std::map<std::string, int32_t> jointMap; // Joint名からIndexを引くための辞書
	std::vector<Joint> joints;               // スケルトンを構成する全Jointのリスト
};

class SkeletonSystem {
public:
	/**
	 * モデルのノード階層からスケルトンを構築する。
	 * DFS（深さ優先探索）により、親のIndexが必ず子より若くなるように構築します。
	 * これにより、更新ループを先頭から回すだけで全階層の行列が正しく計算（フェーズ化）できます。
	 */
	static Skeleton CreateSkeleton(const Model::Node& rootNode);

	/**
	 * 【更新順序の厳守】
	 * 親から順に（DFS順で配列の先頭から）Jointの行列を更新します。
	 * ここで計算された skeletonSpaceMatrix を使って最終的な描画やデバッグ表示を行います。
	 */
	static void Update(Skeleton& skeleton);

	/**
	 * アニメーションデータの補間値を計算し、対象となるJointのtransformを上書きします。
	 * Update関数の前に実行する必要があります。
	 */
	static void ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime);

private:
	// 構築用の再帰関数
	static int32_t CreateJoint(const Model::Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints);
};
