#pragma once

#include "math/Transform.h"

#include <cstdint>
#include <optional>

struct Skeleton;

enum class CG4EvaluationPreset : uint8_t {
	Gameplay,
	Skinning,
	Skeleton,
	WeaponAttachment,
	MultiMeshMaterial,
	GpuParticle,
};

struct CG4EvaluationViewData {
	const Skeleton* skeleton = nullptr;
	bool hasAnimatedObject = false;
	bool hasSkinCluster = false;
	bool computeSkinningEnabled = false;
	bool gpuParticleAvailable = false;
	bool multiMeshMaterialSampleReady = false;
	bool weaponAttachmentReady = false;
	bool weaponAttachmentActive = false;
	bool handParticleAttachmentReady = false;
	bool handParticleAttachmentActive = false;
	bool showWeaponGizmo = true;
	bool showHandParticleGizmo = true;
	bool showAnimatedModel = false;
	bool showMultiMeshMaterialSample = false;
	bool showWeapon = false;
	bool showSkeleton = false;
	bool showLocalAxes = true;
	bool showAllLocalAxes = false;
	bool showJointMarkers = true;
	bool highlightSelectedChain = true;
	bool showParticles = false;
	bool showLegacyTools = false;
	bool animationPlaying = false;
	bool animationSkinningIsolated = false;
	bool animationCrossFadeAvailable = false;
	bool animationCrossFadeActive = false;
	int32_t animationModelIndex = 0;
	int32_t selectedJointIndex = -1;
	int32_t gpuParticleMode = 0;
	float animationTime = 0.0f;
	float animationDuration = 0.0f;
	float animationSpeed = 1.0f;
	float animationCrossFadeDuration = 0.25f;
	float animationCrossFadeProgress = 1.0f;
	uint32_t vertexCount = 0;
	uint32_t indexCount = 0;
	uint32_t meshCount = 0;
	uint32_t materialCount = 0;
	uint32_t jointCount = 0;
	uint32_t weaponVertexCount = 0;
	uint32_t weaponIndexCount = 0;
	uint32_t weaponMeshCount = 0;
	uint32_t weaponMaterialCount = 0;
	Transform weaponLocalTransform{};
	Vector3 weaponSocketScale = { 1.0f, 1.0f, 1.0f };
	Transform handParticleLocalTransform{};
	Vector3 handParticleSocketScale = { 1.0f, 1.0f, 1.0f };
	Vector3 handParticleWorldPosition = { 0.0f, 0.0f, 0.0f };
};

struct CG4EvaluationActions {
	std::optional<CG4EvaluationPreset> preset;
	std::optional<int32_t> animationModelIndex;
	std::optional<int32_t> selectedJointIndex;
	std::optional<bool> showAnimatedModel;
	std::optional<bool> showSkeleton;
	std::optional<bool> showLocalAxes;
	std::optional<bool> showAllLocalAxes;
	std::optional<bool> showJointMarkers;
	std::optional<bool> highlightSelectedChain;
	std::optional<bool> showParticles;
	std::optional<bool> showWeapon;
	std::optional<bool> showWeaponGizmo;
	std::optional<Transform> weaponLocalTransform;
	std::optional<bool> showHandParticleGizmo;
	std::optional<Transform> handParticleLocalTransform;
	std::optional<bool> showLegacyTools;
	std::optional<bool> computeSkinningEnabled;
	std::optional<bool> animationPlaying;
	std::optional<float> animationTime;
	std::optional<float> animationSpeed;
	std::optional<float> animationCrossFadeDuration;
	bool crossFadeToWalk = false;
	bool crossFadeToSneak = false;
	bool resetAnimation = false;
	bool resetWeaponTransform = false;
	bool resetHandParticleTransform = false;
	bool emitGpuParticleSample = false;
};
