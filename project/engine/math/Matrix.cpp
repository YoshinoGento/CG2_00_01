#include "Matrix.h"
#include <cmath>
#include <algorithm>

namespace MatrixMath {

	// --- 行列生成 ---

	// 単位行列の作成
	Matrix4x4 MakeIdentity4x4() {
		Matrix4x4 result = {};
		for (int i = 0; i < 4; ++i) {
			result.m[i][i] = 1.0f;
		}
		return result;
	}

	// 拡大縮小行列の作成
	Matrix4x4 MakeScaleMatrix(const Vector3& scale) {
		Matrix4x4 result = MakeIdentity4x4();
		result.m[0][0] = scale.x;
		result.m[1][1] = scale.y;
		result.m[2][2] = scale.z;
		return result;
	}

	// X軸回転行列の作成
	Matrix4x4 MakeRotateXMatrix(float radian) {
		Matrix4x4 result = MakeIdentity4x4();
		result.m[1][1] = std::cos(radian);
		result.m[1][2] = std::sin(radian);
		result.m[2][1] = -std::sin(radian);
		result.m[2][2] = std::cos(radian);
		return result;
	}

	// Y軸回転行列の作成
	Matrix4x4 MakeRotateYMatrix(float radian) {
		Matrix4x4 result = MakeIdentity4x4();
		result.m[0][0] = std::cos(radian);
		result.m[0][2] = -std::sin(radian);
		result.m[2][0] = std::sin(radian);
		result.m[2][2] = std::cos(radian);
		return result;
	}

	// Z軸回転行列の作成
	Matrix4x4 MakeRotateZMatrix(float radian) {
		Matrix4x4 result = MakeIdentity4x4();
		result.m[0][0] = std::cos(radian);
		result.m[0][1] = std::sin(radian);
		result.m[1][0] = -std::sin(radian);
		result.m[1][1] = std::cos(radian);
		return result;
	}

	// 平行移動行列の作成
	Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
		Matrix4x4 result = MakeIdentity4x4();
		result.m[3][0] = translate.x;
		result.m[3][1] = translate.y;
		result.m[3][2] = translate.z;
		return result;
	}

	// アフィン変換行列の作成 (Scale * Rotate * Translate)
	Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
		Matrix4x4 s = MakeScaleMatrix(scale);
		Matrix4x4 r = Multiply(MakeRotateXMatrix(rotate.x), Multiply(MakeRotateYMatrix(rotate.y), MakeRotateZMatrix(rotate.z)));
		Matrix4x4 t = MakeTranslateMatrix(translate);
		return Multiply(s, Multiply(r, t));
	}

	// --- 行列演算 ---

	// 行列の積 (4x4 * 4x4)
	Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
		Matrix4x4 result = {};
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				for (int k = 0; k < 4; ++k) {
					result.m[i][j] += m1.m[i][k] * m2.m[k][j];
				}
			}
		}
		return result;
	}

	// 転置行列の作成
	Matrix4x4 Transpose(const Matrix4x4& m) {
		Matrix4x4 result;
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				result.m[i][j] = m.m[j][i];
			}
		}
		return result;
	}

	// 逆行列の作成 (全要素計算)
	Matrix4x4 Inverse(const Matrix4x4& m) {
		float det =
			m.m[0][0] * m.m[1][1] * m.m[2][2] * m.m[3][3] + m.m[0][0] * m.m[1][2] * m.m[2][3] * m.m[3][1] + m.m[0][0] * m.m[1][3] * m.m[2][1] * m.m[3][2] -
			m.m[0][0] * m.m[1][3] * m.m[2][2] * m.m[3][1] - m.m[0][0] * m.m[1][2] * m.m[2][1] * m.m[3][3] - m.m[0][0] * m.m[1][1] * m.m[2][3] * m.m[3][2] -
			m.m[0][1] * m.m[1][0] * m.m[2][2] * m.m[3][3] - m.m[0][2] * m.m[1][0] * m.m[2][3] * m.m[3][1] - m.m[0][3] * m.m[1][0] * m.m[2][1] * m.m[3][2] +
			m.m[0][3] * m.m[1][0] * m.m[2][2] * m.m[3][1] + m.m[0][2] * m.m[1][0] * m.m[2][1] * m.m[3][3] + m.m[0][1] * m.m[1][0] * m.m[2][3] * m.m[3][2] +
			m.m[0][1] * m.m[1][2] * m.m[2][0] * m.m[3][3] + m.m[0][2] * m.m[1][3] * m.m[2][0] * m.m[3][1] + m.m[0][3] * m.m[1][1] * m.m[2][0] * m.m[3][2] -
			m.m[0][3] * m.m[1][2] * m.m[2][0] * m.m[3][1] - m.m[0][2] * m.m[1][1] * m.m[2][0] * m.m[3][3] - m.m[0][1] * m.m[1][3] * m.m[2][0] * m.m[3][2] -
			m.m[0][1] * m.m[1][2] * m.m[2][3] * m.m[3][0] - m.m[0][2] * m.m[1][3] * m.m[2][1] * m.m[3][0] - m.m[0][3] * m.m[1][1] * m.m[2][2] * m.m[3][0] +
			m.m[0][3] * m.m[1][2] * m.m[2][1] * m.m[3][0] + m.m[0][2] * m.m[1][1] * m.m[2][3] * m.m[3][0] + m.m[0][1] * m.m[1][3] * m.m[2][2] * m.m[3][0];

		if (std::abs(det) < 1e-7f) return MakeIdentity4x4();

		float invDet = 1.0f / det;
		Matrix4x4 result;

		result.m[0][0] = (m.m[1][1] * m.m[2][2] * m.m[3][3] + m.m[1][2] * m.m[2][3] * m.m[3][1] + m.m[1][3] * m.m[2][1] * m.m[3][2] - m.m[1][3] * m.m[2][2] * m.m[3][1] - m.m[1][2] * m.m[2][1] * m.m[3][3] - m.m[1][1] * m.m[2][3] * m.m[3][2]) * invDet;
		result.m[0][1] = (m.m[0][3] * m.m[2][2] * m.m[3][1] + m.m[0][2] * m.m[2][1] * m.m[3][3] + m.m[0][1] * m.m[2][3] * m.m[3][2] - m.m[0][1] * m.m[2][2] * m.m[3][3] - m.m[0][2] * m.m[2][3] * m.m[3][1] - m.m[0][3] * m.m[2][1] * m.m[3][2]) * invDet;
		result.m[0][2] = (m.m[0][1] * m.m[1][2] * m.m[3][3] + m.m[0][2] * m.m[1][3] * m.m[3][1] + m.m[0][3] * m.m[1][1] * m.m[3][2] - m.m[0][3] * m.m[1][2] * m.m[3][1] - m.m[0][2] * m.m[1][1] * m.m[3][3] - m.m[0][1] * m.m[1][3] * m.m[3][2]) * invDet;
		result.m[0][3] = (m.m[0][3] * m.m[1][2] * m.m[2][1] + m.m[0][2] * m.m[1][1] * m.m[2][3] + m.m[0][1] * m.m[1][3] * m.m[2][2] - m.m[0][1] * m.m[1][2] * m.m[2][3] - m.m[0][2] * m.m[1][3] * m.m[2][1] - m.m[0][3] * m.m[1][1] * m.m[2][2]) * invDet;

		result.m[1][0] = (m.m[1][3] * m.m[2][2] * m.m[3][0] + m.m[1][2] * m.m[2][0] * m.m[3][3] + m.m[1][0] * m.m[2][3] * m.m[3][2] - m.m[1][0] * m.m[2][2] * m.m[3][3] - m.m[1][2] * m.m[2][3] * m.m[3][0] - m.m[1][3] * m.m[2][0] * m.m[3][2]) * invDet;
		result.m[1][1] = (m.m[0][0] * m.m[2][2] * m.m[3][3] + m.m[0][2] * m.m[2][3] * m.m[3][0] + m.m[0][3] * m.m[2][0] * m.m[3][2] - m.m[0][3] * m.m[2][2] * m.m[3][0] - m.m[0][2] * m.m[2][0] * m.m[3][3] - m.m[0][0] * m.m[2][3] * m.m[3][2]) * invDet;
		result.m[1][2] = (m.m[0][3] * m.m[1][2] * m.m[3][0] + m.m[0][2] * m.m[1][0] * m.m[3][3] + m.m[0][0] * m.m[1][3] * m.m[3][2] - m.m[0][0] * m.m[1][2] * m.m[3][3] - m.m[0][2] * m.m[1][3] * m.m[3][0] - m.m[0][3] * m.m[1][0] * m.m[3][2]) * invDet;
		result.m[1][3] = (m.m[0][0] * m.m[1][2] * m.m[2][3] + m.m[0][2] * m.m[1][3] * m.m[2][0] + m.m[0][3] * m.m[1][0] * m.m[2][2] - m.m[0][3] * m.m[1][2] * m.m[2][0] - m.m[0][2] * m.m[1][0] * m.m[2][3] - m.m[0][0] * m.m[1][3] * m.m[2][2]) * invDet;

		result.m[2][0] = (m.m[1][0] * m.m[2][1] * m.m[3][3] + m.m[1][1] * m.m[2][3] * m.m[3][0] + m.m[1][3] * m.m[2][0] * m.m[3][1] - m.m[1][3] * m.m[2][1] * m.m[3][0] - m.m[1][1] * m.m[2][0] * m.m[3][3] - m.m[1][0] * m.m[2][3] * m.m[3][1]) * invDet;
		result.m[2][1] = (m.m[0][3] * m.m[2][1] * m.m[3][0] + m.m[0][1] * m.m[2][0] * m.m[3][3] + m.m[0][0] * m.m[2][3] * m.m[3][1] - m.m[0][0] * m.m[2][1] * m.m[3][3] - m.m[0][1] * m.m[2][3] * m.m[3][0] - m.m[0][3] * m.m[2][0] * m.m[3][1]) * invDet;
		result.m[2][2] = (m.m[0][0] * m.m[1][1] * m.m[3][3] + m.m[0][1] * m.m[1][3] * m.m[3][0] + m.m[0][3] * m.m[1][0] * m.m[3][1] - m.m[0][3] * m.m[1][1] * m.m[3][0] - m.m[0][1] * m.m[1][0] * m.m[3][3] - m.m[0][0] * m.m[1][3] * m.m[3][1]) * invDet;
		result.m[2][3] = (m.m[0][3] * m.m[1][1] * m.m[2][0] + m.m[0][1] * m.m[1][0] * m.m[2][3] + m.m[0][0] * m.m[1][3] * m.m[2][1] - m.m[0][0] * m.m[1][1] * m.m[2][3] - m.m[0][1] * m.m[1][3] * m.m[2][0] - m.m[0][3] * m.m[1][0] * m.m[2][1]) * invDet;

		result.m[3][0] = (m.m[1][3] * m.m[2][1] * m.m[3][0] + m.m[1][1] * m.m[2][0] * m.m[3][2] + m.m[1][0] * m.m[2][2] * m.m[3][1] - m.m[1][0] * m.m[2][1] * m.m[3][2] - m.m[1][1] * m.m[2][2] * m.m[3][0] - m.m[1][3] * m.m[2][0] * m.m[3][1]) * invDet;
		result.m[3][1] = (m.m[0][0] * m.m[2][1] * m.m[3][2] + m.m[0][1] * m.m[2][2] * m.m[3][0] + m.m[0][2] * m.m[2][0] * m.m[3][1] - m.m[0][2] * m.m[2][1] * m.m[3][0] - m.m[0][1] * m.m[2][0] * m.m[3][2] - m.m[0][0] * m.m[2][2] * m.m[3][1]) * invDet;
		result.m[3][2] = (m.m[0][2] * m.m[1][1] * m.m[3][0] + m.m[0][1] * m.m[1][0] * m.m[3][2] + m.m[0][0] * m.m[1][2] * m.m[3][1] - m.m[0][0] * m.m[1][1] * m.m[3][2] - m.m[0][1] * m.m[1][2] * m.m[3][0] - m.m[0][2] * m.m[1][0] * m.m[3][1]) * invDet;
		result.m[3][3] = (m.m[0][0] * m.m[1][1] * m.m[2][2] + m.m[0][1] * m.m[1][2] * m.m[2][0] + m.m[0][2] * m.m[1][0] * m.m[2][1] - m.m[0][0] * m.m[1][2] * m.m[2][1] - m.m[0][1] * m.m[1][0] * m.m[2][2] - m.m[0][2] * m.m[1][1] * m.m[2][0]) * invDet;

		return result;
	}

	// --- 3D描画用行列 ---

	// 透視投影行列 (左手系)
	Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
		Matrix4x4 result = {};
		float t = std::tan(fovY / 2.0f);
		result.m[0][0] = 1.0f / (aspectRatio * t);
		result.m[1][1] = 1.0f / t;
		result.m[2][2] = farClip / (farClip - nearClip);
		result.m[2][3] = 1.0f;
		result.m[3][2] = (-nearClip * farClip) / (farClip - nearClip);
		return result;
	}

	// 正射影行列 (左手系)
	Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
		Matrix4x4 result = MakeIdentity4x4();
		result.m[0][0] = 2.0f / (right - left);
		result.m[1][1] = 2.0f / (top - bottom);
		result.m[2][2] = 1.0f / (farClip - nearClip);
		result.m[3][0] = (left + right) / (left - right);
		result.m[3][1] = (top + bottom) / (bottom - top);
		result.m[3][2] = nearClip / (nearClip - farClip);
		return result;
	}

	// ビューポート行列
	Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) {
		Matrix4x4 result = MakeIdentity4x4();
		result.m[0][0] = width / 2.0f;
		result.m[1][1] = -height / 2.0f;
		result.m[2][2] = maxDepth - minDepth;
		result.m[3][0] = left + width / 2.0f;
		result.m[3][1] = top + height / 2.0f;
		result.m[3][2] = minDepth;
		return result;
	}

	// LookAt行列 (カメラ用)
	Matrix4x4 MakeLookAtMatrix(const Vector3& eye, const Vector3& target, const Vector3& up) {
		Vector3 zAxis = Normalize(target - eye);
		Vector3 xAxis = Normalize(Cross(up, zAxis));
		Vector3 yAxis = Cross(zAxis, xAxis);

		Matrix4x4 result = MakeIdentity4x4();
		result.m[0][0] = xAxis.x; result.m[0][1] = yAxis.x; result.m[0][2] = zAxis.x;
		result.m[1][0] = xAxis.y; result.m[1][1] = yAxis.y; result.m[1][2] = zAxis.y;
		result.m[2][0] = xAxis.z; result.m[2][1] = yAxis.z; result.m[2][2] = zAxis.z;
		result.m[3][0] = -Dot(xAxis, eye);
		result.m[3][1] = -Dot(yAxis, eye);
		result.m[3][2] = -Dot(zAxis, eye);
		return result;
	}

	// --- ベクトル演算 (Vector3) ---

	float Length(const Vector3& v) {
		return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	}

	Vector3 Normalize(const Vector3& v) {
		float len = Length(v);
		if (len < 1e-7f) return { 0, 0, 0 };
		return { v.x / len, v.y / len, v.z / len };
	}

	float Dot(const Vector3& v1, const Vector3& v2) {
		return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
	}

	Vector3 Cross(const Vector3& v1, const Vector3& v2) {
		return {
			v1.y * v2.z - v1.z * v2.y,
			v1.z * v2.x - v1.x * v2.z,
			v1.x * v2.y - v1.y * v2.x
		};
	}

	Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix) {
		Vector3 result;
		result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0] + matrix.m[3][0];
		result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1] + matrix.m[3][1];
		result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2] + matrix.m[3][2];
		float w = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] + vector.z * matrix.m[2][3] + matrix.m[3][3];
		if (std::abs(w) > 1e-7f) {
			result.x /= w; result.y /= w; result.z /= w;
		}
		return result;
	}

	// --- ベクトル演算 (Vector2) ---

	float Length(const Vector2& v) {
		return std::sqrt(v.x * v.x + v.y * v.y);
	}

	Vector2 Normalize(const Vector2& v) {
		float len = Length(v);
		if (len < 1e-7f) return { 0, 0 };
		return { v.x / len, v.y / len };
	}

	float Dot(const Vector2& v1, const Vector2& v2) {
		return v1.x * v2.x + v1.y * v2.y;
	}

	// --- 補間 ---

	float Lerp(float start, float end, float t) {
		return start + (end - start) * t;
	}

	Vector3 Lerp(const Vector3& start, const Vector3& end, float t) {
		return {
			Lerp(start.x, end.x, t),
			Lerp(start.y, end.y, t),
			Lerp(start.z, end.z, t)
		};
	}


	Quaternion Slerp(const Quaternion& q1, const Quaternion& q2, float t) {
		float dot = q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w;
		Quaternion targetQ2 = q2;
		if (dot < 0.0f) {
			dot = -dot;
			targetQ2 = { -q2.x, -q2.y, -q2.z, -q2.w };
		}
		if (dot >= 1.0f - 1e-5f) {
			return { Lerp(q1.x, targetQ2.x, t), Lerp(q1.y, targetQ2.y, t), Lerp(q1.z, targetQ2.z, t), Lerp(q1.w, targetQ2.w, t) };
		}
		float theta = std::acos(dot);
		float sinTheta = std::sin(theta);
		float weight1 = std::sin((1.0f - t) * theta) / sinTheta;
		float weight2 = std::sin(t * theta) / sinTheta;
		return { weight1 * q1.x + weight2 * targetQ2.x, weight1 * q1.y + weight2 * targetQ2.y, weight1 * q1.z + weight2 * targetQ2.z, weight1 * q1.w + weight2 * targetQ2.w };
	}

	Matrix4x4 MakeRotateMatrix(const Quaternion& q) {
		Matrix4x4 result = MakeIdentity4x4();
		result.m[0][0] = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
		result.m[0][1] = 2.0f * (q.x * q.y + q.w * q.z);
		result.m[0][2] = 2.0f * (q.x * q.z - q.w * q.y);
		result.m[1][0] = 2.0f * (q.x * q.y - q.w * q.z);
		result.m[1][1] = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
		result.m[1][2] = 2.0f * (q.y * q.z + q.w * q.x);
		result.m[2][0] = 2.0f * (q.x * q.z + q.w * q.y);
		result.m[2][1] = 2.0f * (q.y * q.z - q.w * q.x);
		result.m[2][2] = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
		return result;
	}
}