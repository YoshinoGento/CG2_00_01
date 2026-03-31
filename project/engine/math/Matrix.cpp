#include "Matrix.h"
#include <cmath>
#include <algorithm>

/**
 * 単位行列の作成
 * 数値でいう「1」に相当する、何もしない行列です。
 */
Matrix4x4 MatrixMath::MakeIdentity4x4() {
	Matrix4x4 result = {};
	for (int i = 0; i < 4; ++i) result.m[i][i] = 1.0f;
	return result;
}

/**
 * 拡大縮小行列
 */
Matrix4x4 MatrixMath::MakeScaleMatrix(const Vector3& scale) {
	Matrix4x4 result = MakeIdentity4x4();
	result.m[0][0] = scale.x;
	result.m[1][1] = scale.y;
	result.m[2][2] = scale.z;
	return result;
}

/**
 * X軸回転行列
 */
Matrix4x4 MatrixMath::MakeRotateXMatrix(float radian) {
	Matrix4x4 result = MakeIdentity4x4();
	result.m[1][1] = std::cos(radian);
	result.m[1][2] = std::sin(radian);
	result.m[2][1] = -std::sin(radian);
	result.m[2][2] = std::cos(radian);
	return result;
}

/**
 * Y軸回転行列
 */
Matrix4x4 MatrixMath::MakeRotateYMatrix(float radian) {
	Matrix4x4 result = MakeIdentity4x4();
	result.m[0][0] = std::cos(radian);
	result.m[0][2] = -std::sin(radian);
	result.m[2][0] = std::sin(radian);
	result.m[2][2] = std::cos(radian);
	return result;
}

/**
 * Z軸回転行列
 */
Matrix4x4 MatrixMath::MakeRotateZMatrix(float radian) {
	Matrix4x4 result = MakeIdentity4x4();
	result.m[0][0] = std::cos(radian);
	result.m[0][1] = std::sin(radian);
	result.m[1][0] = -std::sin(radian);
	result.m[1][1] = std::cos(radian);
	return result;
}

/**
 * 平行移動行列
 */
Matrix4x4 MatrixMath::MakeTranslateMatrix(const Vector3& translate) {
	Matrix4x4 result = MakeIdentity4x4();
	result.m[3][0] = translate.x;
	result.m[3][1] = translate.y;
	result.m[3][2] = translate.z;
	return result;
}

/**
 * アフィン変換行列
 * スケール・回転・移動をこの順番で合成します。
 */
Matrix4x4 MatrixMath::MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
	Matrix4x4 matScale = MakeScaleMatrix(scale);
	Matrix4x4 matRot = Multiply(MakeRotateXMatrix(rotate.x), Multiply(MakeRotateYMatrix(rotate.y), MakeRotateZMatrix(rotate.z)));
	Matrix4x4 matTrans = MakeTranslateMatrix(translate);
	return Multiply(matScale, Multiply(matRot, matTrans));
}

/**
 * 行列の積
 */
Matrix4x4 MatrixMath::Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result;
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			result.m[i][j] = 0;
			for (int k = 0; k < 4; ++k) {
				result.m[i][j] += m1.m[i][k] * m2.m[k][j];
			}
		}
	}
	return result;
}

/**
 * 逆行列 (ガウス・ジョルダン法)
 */
Matrix4x4 MatrixMath::Inverse(const Matrix4x4& m) {
	Matrix4x4 res = MakeIdentity4x4();
	float a[4][4];
	for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) a[i][j] = m.m[i][j];

	for (int i = 0; i < 4; i++) {
		int pivot = i;
		for (int j = i + 1; j < 4; j++) if (std::abs(a[j][i]) > std::abs(a[pivot][i])) pivot = j;
		std::swap(a[i], a[pivot]);
		std::swap(res.m[i], res.m[pivot]);

		float div = a[i][i];
		for (int j = 0; j < 4; j++) { a[i][j] /= div; res.m[i][j] /= div; }
		for (int j = 0; j < 4; j++) {
			if (i != j) {
				float mul = a[j][i];
				for (int k = 0; k < 4; k++) { a[j][k] -= a[i][k] * mul; res.m[j][k] -= res.m[i][k] * mul; }
			}
		}
	}
	return res;
}

/**
 * 透視投影行列 (3D描画用)
 */
Matrix4x4 MatrixMath::MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
	float cot = (1.0f / std::tan(fovY / 2.0f));
	Matrix4x4 result = {};
	result.m[0][0] = 1.0f / aspectRatio * cot;
	result.m[1][1] = cot;
	result.m[2][2] = farClip / (farClip - nearClip);
	result.m[2][3] = 1.0f;
	result.m[3][2] = (-nearClip * farClip) / (farClip - nearClip);
	return result;
}

/**
 * 正射影行列 (2D描画用)
 */
Matrix4x4 MatrixMath::MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
	Matrix4x4 result = {};
	result.m[0][0] = 2.0f / (right - left);
	result.m[1][1] = 2.0f / (top - bottom);
	result.m[2][2] = 1.0f / (farClip - nearClip);
	result.m[3][0] = (left + right) / (left - right);
	result.m[3][1] = (top + bottom) / (bottom - top);
	result.m[3][2] = nearClip / (nearClip - farClip);
	result.m[3][3] = 1.0f;
	return result;
}

/**
 * ビューポート行列
 */
Matrix4x4 MatrixMath::MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) {
	Matrix4x4 result = MakeIdentity4x4();
	result.m[0][0] = width / 2.0f;
	result.m[1][1] = -height / 2.0f;
	result.m[2][2] = maxDepth - minDepth;
	result.m[3][0] = left + (width / 2.0f);
	result.m[3][1] = top + (height / 2.0f);
	result.m[3][2] = minDepth;
	return result;
}

/**
 * 座標変換
 */
Vector3 MatrixMath::Transform(const Vector3& vector, const Matrix4x4& matrix) {
	Vector3 result;
	result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0] + matrix.m[3][0];
	result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1] + matrix.m[3][1];
	result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2] + matrix.m[3][2];
	float w = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] + vector.z * matrix.m[2][3] + matrix.m[3][3];
	result.x /= w; result.y /= w; result.z /= w;
	return result;
}

/**
 * ベクトルの長さ
 */
float MatrixMath::Length(const Vector3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
float MatrixMath::Length(const Vector2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }

/**
 * 正規化
 */
Vector3 MatrixMath::Normalize(const Vector3& v) {
	float len = Length(v);
	if (len == 0.0f) return v;
	return v / len;
}

Vector2 MatrixMath::Normalize(const Vector2& v) {
	float len = Length(v);
	if (len == 0.0f) return v;
	return v / len;
}

/**
 * 内積
 */
float MatrixMath::Dot(const Vector3& v1, const Vector3& v2) { return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z; }

/**
 * 外積 (修正済)
 */
Vector3 MatrixMath::Cross(const Vector3& v1, const Vector3& v2) {
	return {
		v1.y * v2.z - v1.z * v2.y,
		v1.z * v2.x - v1.x * v2.z,
		v1.x * v2.y - v1.y * v2.x
	};
}