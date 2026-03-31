#pragma once
#include "Struct.h"
#include "Transform.h"

/**
 * Matrix4x4 構造体
 */
struct Matrix4x4 {
	float m[4][4];
};

/**
 * MatrixMath 名前空間
 * すべての数学関数をここに集約します。
 */
namespace MatrixMath {

	// --- 行列生成 (基本) ---
	Matrix4x4 MakeIdentity4x4(); // 単位行列
	Matrix4x4 MakeScaleMatrix(const Vector3& scale); // 拡大縮小
	Matrix4x4 MakeRotateXMatrix(float radian); // X軸回転
	Matrix4x4 MakeRotateYMatrix(float radian); // Y軸回転
	Matrix4x4 MakeRotateZMatrix(float radian); // Z軸回転
	Matrix4x4 MakeTranslateMatrix(const Vector3& translate); // 平行移動
	Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate); // 全部入り

	// --- 行列演算 ---
	Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2); // 積
	Matrix4x4 Inverse(const Matrix4x4& m); // 逆行列
	Matrix4x4 Transpose(const Matrix4x4& m); // 転置行列

	// --- 3D描画用行列 ---
	Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip); // 3Dカメラ
	Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip); // 2D用
	Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth); // 画面座標変換

	// --- ベクトル操作 ---
	Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix); // 座標変換
	float Length(const Vector3& v);     // ベクトルの長さ
	Vector3 Normalize(const Vector3& v); // 正規化（長さを1にする）
	float Dot(const Vector3& v1, const Vector3& v2);    // 内積（角度や光の計算）
	Vector3 Cross(const Vector3& v1, const Vector3& v2); // 外積（面の向きを求める）

	// --- Vector2用ヘルパー ---
	float Length(const Vector2& v);
	Vector2 Normalize(const Vector2& v);
};