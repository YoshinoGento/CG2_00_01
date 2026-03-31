#pragma once

/**
 * Vector2 構造体
 * 2次元座標（スプライト、画面座標、UVなど）に使用。
 */
struct Vector2 {
    float x;
    float y;

    // --- 算術演算子 (加減乗除) ---
    Vector2 operator+(const Vector2& obj) const { return { x + obj.x, y + obj.y }; }
    Vector2 operator-(const Vector2& obj) const { return { x - obj.x, y - obj.y }; }
    Vector2 operator*(float scalar) const { return { x * scalar, y * scalar }; }
    Vector2 operator/(float scalar) const { return { x / scalar, y / scalar }; }

    // --- 代入演算子 ---
    Vector2& operator+=(const Vector2& obj) { x += obj.x; y += obj.y; return *this; }
    Vector2& operator-=(const Vector2& obj) { x -= obj.x; y -= obj.y; return *this; }
    Vector2& operator*=(float scalar) { x *= scalar; y *= scalar; return *this; }
    Vector2& operator/=(float scalar) { x /= scalar; y /= scalar; return *this; }
};

/**
 * Vector3 構造体
 * 3次元座標（ワールド座標、法線、回転角など）に使用。
 */
struct Vector3 {
    float x;
    float y;
    float z;

    // --- 算術演算子 (加減乗除) ---
    Vector3 operator+(const Vector3& obj) const { return { x + obj.x, y + obj.y, z + obj.z }; }
    Vector3 operator-(const Vector3& obj) const { return { x - obj.x, y - obj.y, z - obj.z }; }
    Vector3 operator*(float scalar) const { return { x * scalar, y * scalar, z * scalar }; }
    Vector3 operator/(float scalar) const { return { x / scalar, y / scalar, z / scalar }; }

    // --- 代入演算子 ---
    Vector3& operator+=(const Vector3& obj) { x += obj.x; y += obj.y; z += obj.z; return *this; }
    Vector3& operator-=(const Vector3& obj) { x -= obj.x; y -= obj.y; z -= obj.z; return *this; }
    Vector3& operator*=(float scalar) { x *= scalar; y *= scalar; return *this; }
    Vector3& operator/=(float scalar) { x /= scalar; y /= scalar; return *this; }

    // 単項演算子（符号を反転させる）
    Vector3 operator-() const { return { -x, -y, -z }; }
};

/**
 * Vector4 構造体
 * 色(RGBA)や、同次座標系での計算に使用。
 */
struct Vector4 {
    float x;
    float y;
    float z;
    float w;
};

/**
 * Sphere (球) 構造体
 * 当たり判定などで使用。
 */
struct Sphere {
    Vector3 center; // 中心点
    float radius;   // 半径
};