#pragma once

#include <cstdint>
#include <string>
#include <vector>

class Object3d;

class DebugEditorWindow {
public:
	enum class SpawnKind {
		StaticModel,
		SpherePrimitive,
		AnimatedModel,
	};

	struct SpawnRequest {
		SpawnKind kind = SpawnKind::StaticModel;
		std::string displayName;
		std::string modelPath;
		std::string animationDirectory;
		std::string animationFile;
	};

	void ClearTargets();
	void AddTarget(const char* name, Object3d* object);
	void Draw(bool* showSceneDebugWindow = nullptr, bool* showFarmEditorWindow = nullptr);
	Object3d* GetSelectedObject() const;
	bool ConsumeSpawnRequest(SpawnRequest& outRequest);
	bool IsObjectSelectionModeEnabled() const { return objectSelectionModeEnabled_; }
	void SelectObject(Object3d* object);

private:
	enum class EditorMode {
		Select,
		Place,
		Animation,
		Light,
		Particle,
		Farm,
	};

	struct Target {
		std::string name;
		Object3d* object = nullptr;
	};

	void DrawToolbar(bool* showSceneDebugWindow, bool* showFarmEditorWindow);
	void DrawModeButton(const char* label, EditorMode mode);
	void DrawMainLayout();
	void DrawLeftPanel();
	void DrawInspectorPanel();
	void DrawObjectList(bool animationOnly);
	void DrawObjectPalette();
	void DrawObjectInspector(Object3d* object, const Target* target);
	void DrawAnimationInspector(Object3d* object, const Target* target);
	void DrawLightInspector();
	void DrawParticleInspector();
	void DrawFarmInspector(bool* showFarmEditorWindow);
	void SelectTarget(int32_t index);
	void RequestSpawnSelectedPaletteItem();
	void ClampSelection();
	const Target* GetSelectedTarget() const;
	const char* GetObjectTypeName(const Object3d* object) const;

	std::vector<Target> targets_;
	int32_t selectedTargetIndex_ = -1;
	int32_t selectedPaletteIndex_ = 0;
	EditorMode editorMode_ = EditorMode::Select;
	bool objectSelectionModeEnabled_ = true;
	bool showAdvanced_ = false;
	bool hasPendingSpawnRequest_ = false;
	SpawnRequest pendingSpawnRequest_{};
};
