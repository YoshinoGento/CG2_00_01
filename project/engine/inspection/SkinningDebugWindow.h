#pragma once

#include "math/Matrix.h"
#include <cstdint>

class Model;
class Object3d;
struct Skeleton;

class SkinningDebugWindow {
public:
	void Draw(Object3d* targetObject);

private:
	struct ValidationResult {
		uint32_t checkedVertexCount = 0;
		uint32_t invalidWeightSumCount = 0;
		uint32_t invalidJointIndexCount = 0;
		uint32_t noInfluenceVertexCount = 0;
		bool paletteCountMismatch = false;
	};

	void DrawSummary(Object3d* targetObject, const Skeleton* skeleton, const Model* model);
	void DrawErrorCheck(const Skeleton* skeleton, const Model* model);
	void DrawSkeletonInfo(const Skeleton* skeleton);
	void DrawSelectedJoint(const Skeleton* skeleton);
	void DrawVertexInfluence(const Skeleton* skeleton, const Model* model);
	void DrawMatrixPalette(Object3d* targetObject, const Skeleton* skeleton, const Model* model);
	ValidationResult Validate(const Skeleton* skeleton, const Model* model) const;
	void ClampSelections(const Skeleton* skeleton, const Model* model);
	void DrawMatrix4x4(const char* label, const Matrix4x4& matrix) const;
	uint32_t GetInfluenceCount(const Model* model) const;
	uint32_t GetPaletteCount(const Model* model) const;

	int32_t selectedJointIndex_ = -1;
	int32_t selectedVertexIndex_ = 0;
	int32_t selectedPaletteIndex_ = 0;
	bool showOnlyErrors_ = false;
	ValidationResult validationResult_{};
	bool hasValidationResult_ = false;
};
