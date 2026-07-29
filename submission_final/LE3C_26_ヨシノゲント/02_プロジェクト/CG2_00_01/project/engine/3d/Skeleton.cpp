#include "3d/Skeleton.h"
#include <algorithm>
#include <cassert>
#include <cmath>

namespace {
QuaternionTransform SampleJointTransform(
	const Joint& joint,
	const Animation& animation,
	float animationTime) {
	QuaternionTransform result = joint.transform;
	const auto nodeAnimationIt = animation.nodeAnimations.find(joint.name);
	if (nodeAnimationIt == animation.nodeAnimations.end()) {
		return result;
	}

	const NodeAnimation& nodeAnimation = nodeAnimationIt->second;
	if (!nodeAnimation.translate.empty()) {
		result.translate = CalculateValue(nodeAnimation.translate, animationTime);
	}
	if (!nodeAnimation.rotate.empty()) {
		result.rotate = CalculateValue(nodeAnimation.rotate, animationTime);
	}
	if (!nodeAnimation.scale.empty()) {
		result.scale = CalculateValue(nodeAnimation.scale, animationTime);
	}
	return result;
}
}

int32_t SkeletonSystem::CreateJoint(const Model::Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints) {
	Joint joint;
	joint.name = node.name;
	joint.localMatrix = node.localMatrix; // モデルファイルから読み込んだ初期行列
	joint.skeletonSpaceMatrix = MatrixMath::MakeIdentity4x4();
	
	// 拡張されたモデルノードからトランスフォーム情報を引き継ぐ
	joint.transform = node.transform;

	joint.parent = parent;
	joint.index = static_cast<int32_t>(joints.size());
	joints.push_back(joint);

	int32_t currentJointIndex = joint.index;

	for (const auto& childNode : node.children) {
		int32_t childIndex = CreateJoint(childNode, currentJointIndex, joints);
		// push_backによる再確保でポインタが無効になる可能性があるため、配列のインデックスでアクセス
		joints[currentJointIndex].children.push_back(childIndex);
	}

	return currentJointIndex;
}

Skeleton SkeletonSystem::CreateSkeleton(const Model::Node& rootNode) {
	Skeleton skeleton;
	skeleton.root = CreateJoint(rootNode, std::nullopt, skeleton.joints);

	// 辞書の作成
	for (const Joint& joint : skeleton.joints) {
		skeleton.jointMap[joint.name] = joint.index;
	}

	// 初期更新
	Update(skeleton);

	return skeleton;
}

void SkeletonSystem::Update(Skeleton& skeleton) {
	// 【解説】
	// すべてのJointを更新します。
	// CreateSkeletonでのDFS探索により、配列jointsの中身は必ず親が若番になります。
	// そのため、配列の先頭から順番に処理するだけで、子が参照する親の行列は計算済みであることが保証されます。
	for (Joint& joint : skeleton.joints) {
		// transformからlocalMatrixを更新（ApplyAnimationでtransformが書き換わっている前提）
		joint.localMatrix = MatrixMath::Multiply(MatrixMath::MakeScaleMatrix(joint.transform.scale),
			MatrixMath::Multiply(MatrixMath::MakeRotateMatrix(joint.transform.rotate),
				MatrixMath::MakeTranslateMatrix(joint.transform.translate)));

		if (joint.parent) {
			// 親がいれば親のskeletonSpaceMatrixを掛ける
			// これにより「Local空間」→「Skeleton空間（モデル全体）」への座標系変換が成立します。
			joint.skeletonSpaceMatrix = MatrixMath::Multiply(joint.localMatrix, skeleton.joints[*joint.parent].skeletonSpaceMatrix);
		} else {
			// 親がいない（Root）ならそのまま
			joint.skeletonSpaceMatrix = joint.localMatrix;
		}
	}
}

void SkeletonSystem::ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime) {
	for (Joint& joint : skeleton.joints) {
		auto it = animation.nodeAnimations.find(joint.name);
		if (it != animation.nodeAnimations.end()) {
			const NodeAnimation& rootAnim = it->second;
			joint.transform.translate = CalculateValue(rootAnim.translate, animationTime);
			joint.transform.rotate = CalculateValue(rootAnim.rotate, animationTime);
			joint.transform.scale = CalculateValue(rootAnim.scale, animationTime);
		}
	}
}

void SkeletonSystem::ApplyAnimationBlend(
	Skeleton& skeleton,
	const Animation& sourceAnimation,
	float sourceTime,
	const Animation& targetAnimation,
	float targetTime,
	float blendWeight) {
	if (!std::isfinite(blendWeight)) {
		blendWeight = 1.0f;
	}
	blendWeight = std::clamp(blendWeight, 0.0f, 1.0f);

	for (Joint& joint : skeleton.joints) {
		const QuaternionTransform sourceTransform = SampleJointTransform(joint, sourceAnimation, sourceTime);
		const QuaternionTransform targetTransform = SampleJointTransform(joint, targetAnimation, targetTime);
		joint.transform.translate = MatrixMath::Lerp(
			sourceTransform.translate, targetTransform.translate, blendWeight);
		joint.transform.rotate = MatrixMath::Slerp(
			sourceTransform.rotate, targetTransform.rotate, blendWeight);
		joint.transform.scale = MatrixMath::Lerp(
			sourceTransform.scale, targetTransform.scale, blendWeight);
	}
}
