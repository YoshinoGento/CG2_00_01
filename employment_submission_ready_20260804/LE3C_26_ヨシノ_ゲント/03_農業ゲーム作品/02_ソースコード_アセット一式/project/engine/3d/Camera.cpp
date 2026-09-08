#include "3d/Camera.h"
#include "base/WinApp.h"

Camera::Camera()
	: transform_({ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -10.0f} })
	, fovY_(0.45f)
	, aspectRatio_(float(WinApp::kClientWidth) / float(WinApp::kClientHeight))
	, nearClip_(0.1f)
	, farClip_(1000.0f) {
	Update();
}

void Camera::Update() {
	// カメラのワールド行列作成
	Matrix4x4 rotateMatrixX = MatrixMath::MakeRotateXMatrix(transform_.rotate.x);
	Matrix4x4 rotateMatrixY = MatrixMath::MakeRotateYMatrix(transform_.rotate.y);
	Matrix4x4 rotateMatrixZ = MatrixMath::MakeRotateZMatrix(transform_.rotate.z);
	Matrix4x4 rotateMatrix = MatrixMath::Multiply(rotateMatrixX, MatrixMath::Multiply(rotateMatrixY, rotateMatrixZ));
	Matrix4x4 translateMatrix = MatrixMath::MakeTranslateMatrix(transform_.translate);
	worldMatrix_ = MatrixMath::Multiply(rotateMatrix, translateMatrix);

	// ビュー行列作成 (カメラのワールド行列の逆行列)
	viewMatrix_ = MatrixMath::Inverse(worldMatrix_);

	// プロジェクション行列作成
	projectionMatrix_ = MatrixMath::MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);

	// 合成行列 (View * Projection)
	viewProjectionMatrix_ = MatrixMath::Multiply(viewMatrix_, projectionMatrix_);
}