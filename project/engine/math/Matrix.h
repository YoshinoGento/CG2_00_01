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
 * 3Dゲームエンジンに必要な数学関数を網羅しています。
 */
namespace MatrixMath {

	// --- 行列生成 (基本) ---
	Matrix4x4 MakeIdentity4x4();
	Matrix4x4 MakeScaleMatrix(const Vector3& scale);
	Matrix4x4 MakeRotateXMatrix(float radian);
	Matrix4x4 MakeRotateYMatrix(float radian);
	Matrix4x4 MakeRotateZMatrix(float radian);
	Matrix4x4 MakeTranslateMatrix(const Vector3& translate);

	// アフィン変換行列 (Scale * Rotate * Translate)
	Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

	// --- 行列演算 ---
	Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);
	Matrix4x4 Inverse(const Matrix4x4& m);
	Matrix4x4 Transpose(const Matrix4x4& m);

	// --- Quaternion演算 ---
	Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t);
	Matrix4x4 MakeRotateMatrix(const Quaternion& quaternion);


	// --- 3D描画用行列 ---
	// 透視投影行列 (3Dカメラ用)
	Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);
	// 平行投影行列 (2D・UI用)
	Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);
	// ビューポート行列 (画面座標変換用)
	Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);

	// カメラ用：指定した位置を向く行列
	Matrix4x4 MakeLookAtMatrix(const Vector3& eye, const Vector3& target, const Vector3& up);

	// --- ベクトル演算 (Vector3) ---
	float Length(const Vector3& v);                      // ベクトルの長さ
	Vector3 Normalize(const Vector3& v);                  // 正規化 (長さを1にする)
	float Dot(const Vector3& v1, const Vector3& v2);     // 内積 (角度・照明計算)
	Vector3 Cross(const Vector3& v1, const Vector3& v2);   // 外積 (法線・回転軸計算)
	Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix); // 座標変換

	// --- ベクトル演算 (Vector2) ---
	float Length(const Vector2& v);
	Vector2 Normalize(const Vector2& v);
	float Dot(const Vector2& v1, const Vector2& v2);

	// --- 便利な補間関数 ---
	float Lerp(float start, float end, float t);         // 線形補間
	Vector3 Lerp(const Vector3& start, const Vector3& end, float t);

	Vector3 Slerp(const Vector3& v1, const Vector3& v2, float t); // Vector用（Lerpでも可）
	Quaternion Slerp(const Quaternion& q1, const Quaternion& q2, float t);
	Matrix4x4 MakeRotateMatrix(const Quaternion& q);
}