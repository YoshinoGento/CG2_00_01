#pragma once

#include "math/Struct.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace level {

struct TransformData {
	Vector3 translation = { 0.0f, 0.0f, 0.0f };
	Vector3 rotation = { 0.0f, 0.0f, 0.0f };
	Vector3 scaling = { 1.0f, 1.0f, 1.0f };
};

struct ColliderData {
	std::string type;
	Vector3 center = { 0.0f, 0.0f, 0.0f };
	Vector3 size = { 1.0f, 1.0f, 1.0f };
};

struct ObjectData {
	std::string type;
	std::string name;
	std::string fileName;
	TransformData transform;
	bool disabled = false;
	bool hasCollider = false;
	ColliderData collider;
	int32_t eventId = -1;
	int32_t requiredKeyId = -1;
	std::string spawnType;
	std::string enemyType;
	std::string gimmickType;
	std::string gameplayRole;
};

struct LevelData {
	std::string name;
	std::vector<ObjectData> objects;
};

class LevelLoader {
public:
	static std::unique_ptr<LevelData> Load(const std::string& levelName);
	static bool Load(const std::string& levelName, LevelData& outLevelData);
};

}
