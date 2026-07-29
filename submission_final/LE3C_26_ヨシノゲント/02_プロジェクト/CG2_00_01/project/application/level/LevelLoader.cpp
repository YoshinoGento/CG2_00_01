#include "level/LevelLoader.h"

#include "base/Logger.h"
#include "io/JsonFile.h"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace level {
namespace {

constexpr const char* kBaseDirectory = "Resources/levels/";
constexpr const char* kExtension = ".json";
constexpr const char* kSceneRootName = "scene";
constexpr const char* kMeshTypeName = "MESH";

std::string BuildLevelPath(const std::string& levelName)
{
	const bool hasJsonExtension =
		levelName.size() >= 5 &&
		levelName.substr(levelName.size() - 5) == kExtension;
	const bool hasDirectory =
		levelName.find('/') != std::string::npos ||
		levelName.find('\\') != std::string::npos;

	if (hasDirectory) {
		return levelName;
	}

	return std::string(kBaseDirectory) + levelName + (hasJsonExtension ? "" : kExtension);
}

void LogLoadError(const std::string& message)
{
	Logger::Log("LevelLoader: " + message + "\n");
}

bool ReadRequiredString(
	const nlohmann::json& json,
	const char* key,
	const std::string& context,
	std::string& outValue)
{
	if (!json.contains(key) || !json[key].is_string()) {
		LogLoadError(context + " requires string key: " + key);
		return false;
	}

	outValue = json[key].get<std::string>();
	return true;
}

bool ReadOptionalString(
	const nlohmann::json& json,
	const char* key,
	const std::string& context,
	std::string& outValue)
{
	if (!json.contains(key)) {
		return true;
	}
	if (!json[key].is_string()) {
		LogLoadError(context + " optional key must be string: " + key);
		return false;
	}

	outValue = json[key].get<std::string>();
	return true;
}

bool ReadOptionalBool(
	const nlohmann::json& json,
	const char* key,
	const std::string& context,
	bool& outValue)
{
	if (!json.contains(key)) {
		return true;
	}
	if (!json[key].is_boolean()) {
		LogLoadError(context + " optional key must be bool: " + key);
		return false;
	}

	outValue = json[key].get<bool>();
	return true;
}

bool ReadOptionalInt(
	const nlohmann::json& json,
	const char* key,
	const std::string& context,
	int32_t& outValue)
{
	if (!json.contains(key)) {
		return true;
	}
	if (!json[key].is_number_integer()) {
		LogLoadError(context + " optional key must be integer: " + key);
		return false;
	}

	outValue = json[key].get<int32_t>();
	return true;
}

bool ReadVector3(
	const nlohmann::json& json,
	const char* key,
	const std::string& context,
	Vector3& outValue)
{
	if (!json.contains(key) || !json[key].is_array() || json[key].size() < 3) {
		LogLoadError(context + " requires array[3] key: " + key);
		return false;
	}

	for (std::size_t i = 0; i < 3; ++i) {
		if (!json[key][i].is_number()) {
			LogLoadError(context + " has non-number element in key: " + key);
			return false;
		}
	}

	outValue = {
		json[key][0].get<float>(),
		json[key][1].get<float>(),
		json[key][2].get<float>(),
	};
	return true;
}

Vector3 ConvertBlenderVector(const Vector3& blenderVector)
{
	return {
		blenderVector.x,
		blenderVector.z,
		blenderVector.y,
	};
}

TransformData ConvertBlenderTransform(
	const Vector3& blenderTranslation,
	const Vector3& blenderRotation,
	const Vector3& blenderScaling)
{
	TransformData transform{};
	transform.translation = ConvertBlenderVector(blenderTranslation);
	transform.rotation = {
		-blenderRotation.x,
		-blenderRotation.z,
		-blenderRotation.y,
	};
	transform.scaling = ConvertBlenderVector(blenderScaling);
	return transform;
}

bool ReadTransform(
	const nlohmann::json& objectJson,
	const std::string& context,
	TransformData& outTransform)
{
	if (!objectJson.contains("transform") || !objectJson["transform"].is_object()) {
		LogLoadError(context + " requires object key: transform");
		return false;
	}

	const nlohmann::json& transformJson = objectJson["transform"];
	Vector3 blenderTranslation{};
	Vector3 blenderRotation{};
	Vector3 blenderScaling{ 1.0f, 1.0f, 1.0f };

	if (!ReadVector3(transformJson, "translation", context + ".transform", blenderTranslation) ||
		!ReadVector3(transformJson, "rotation", context + ".transform", blenderRotation) ||
		!ReadVector3(transformJson, "scaling", context + ".transform", blenderScaling)) {
		return false;
	}

	outTransform = ConvertBlenderTransform(blenderTranslation, blenderRotation, blenderScaling);
	return true;
}

bool ReadOptionalCollider(
	const nlohmann::json& objectJson,
	const std::string& context,
	ObjectData& outObject)
{
	if (!objectJson.contains("collider")) {
		return true;
	}
	if (!objectJson["collider"].is_object()) {
		LogLoadError(context + ".collider must be an object");
		return false;
	}

	const nlohmann::json& colliderJson = objectJson["collider"];
	ColliderData collider{};
	collider.type = "BOX";
	if (!ReadOptionalString(colliderJson, "type", context + ".collider", collider.type)) {
		return false;
	}

	Vector3 blenderCenter{};
	Vector3 blenderSize{ 1.0f, 1.0f, 1.0f };
	if (colliderJson.contains("center")) {
		if (!ReadVector3(colliderJson, "center", context + ".collider", blenderCenter)) {
			return false;
		}
		collider.center = ConvertBlenderVector(blenderCenter);
	}
	if (colliderJson.contains("size")) {
		if (!ReadVector3(colliderJson, "size", context + ".collider", blenderSize)) {
			return false;
		}
		collider.size = ConvertBlenderVector(blenderSize);
	}

	outObject.hasCollider = true;
	outObject.collider = std::move(collider);
	return true;
}

std::size_t CountObjectNodes(const nlohmann::json& objectsJson)
{
	if (!objectsJson.is_array()) {
		return 0;
	}

	std::size_t count = 0;
	for (const nlohmann::json& objectJson : objectsJson) {
		if (!objectJson.is_object()) {
			continue;
		}
		++count;
		if (objectJson.contains("children")) {
			count += CountObjectNodes(objectJson["children"]);
		}
	}
	return count;
}

bool ReadObject(
	const nlohmann::json& objectJson,
	const std::string& context,
	ObjectData& outObject)
{
	if (!objectJson.is_object()) {
		LogLoadError(context + " must be an object");
		return false;
	}

	if (!ReadRequiredString(objectJson, "type", context, outObject.type)) {
		return false;
	}

	if (objectJson.contains("name") && objectJson["name"].is_string()) {
		outObject.name = objectJson["name"].get<std::string>();
	}

	if (!ReadTransform(objectJson, context, outObject.transform)) {
		return false;
	}

	if (!ReadOptionalBool(objectJson, "disabled", context, outObject.disabled) ||
		!ReadOptionalInt(objectJson, "event_id", context, outObject.eventId) ||
		!ReadOptionalInt(objectJson, "required_key_id", context, outObject.requiredKeyId) ||
		!ReadOptionalString(objectJson, "spawn_type", context, outObject.spawnType) ||
		!ReadOptionalString(objectJson, "enemy_type", context, outObject.enemyType) ||
		!ReadOptionalString(objectJson, "gimmick_type", context, outObject.gimmickType) ||
		!ReadOptionalString(objectJson, "gameplay_role", context, outObject.gameplayRole) ||
		!ReadOptionalCollider(objectJson, context, outObject)) {
		return false;
	}

	if (outObject.type == kMeshTypeName) {
		if (!ReadRequiredString(objectJson, "file_name", context, outObject.fileName)) {
			return false;
		}
	}

	return true;
}

bool ReadObjectsRecursive(
	const nlohmann::json& objectsJson,
	const std::string& context,
	LevelData& outLevelData)
{
	if (!objectsJson.is_array()) {
		LogLoadError(context + " must be an array");
		return false;
	}

	bool succeeded = true;
	for (std::size_t i = 0; i < objectsJson.size(); ++i) {
		const std::string objectContext = context + "[" + std::to_string(i) + "]";
		ObjectData objectData{};
		if (ReadObject(objectsJson[i], objectContext, objectData)) {
			outLevelData.objects.push_back(std::move(objectData));
		} else {
			succeeded = false;
		}

		if (objectsJson[i].is_object() && objectsJson[i].contains("children")) {
			if (!ReadObjectsRecursive(objectsJson[i]["children"], objectContext + ".children", outLevelData)) {
				succeeded = false;
			}
		}
	}

	return succeeded;
}

}

std::unique_ptr<LevelData> LevelLoader::Load(const std::string& levelName)
{
	std::unique_ptr<LevelData> levelData = std::make_unique<LevelData>();
	if (!Load(levelName, *levelData)) {
		return nullptr;
	}
	return levelData;
}

bool LevelLoader::Load(const std::string& levelName, LevelData& outLevelData)
{
	const std::string path = BuildLevelPath(levelName);

	nlohmann::json rootJson;
	if (!JsonFile::Load(path, rootJson)) {
		LogLoadError("failed to load " + path);
		return false;
	}

	if (!rootJson.is_object()) {
		LogLoadError(path + " root must be an object");
		return false;
	}

	LevelData levelData{};
	if (!ReadRequiredString(rootJson, "name", "root", levelData.name)) {
		return false;
	}
	if (levelData.name != kSceneRootName) {
		LogLoadError(path + " root name must be scene");
		return false;
	}
	if (!rootJson.contains("objects") || !rootJson["objects"].is_array()) {
		LogLoadError(path + " requires array key: objects");
		return false;
	}

	levelData.objects.reserve(CountObjectNodes(rootJson["objects"]));
	const bool succeeded = ReadObjectsRecursive(rootJson["objects"], "root.objects", levelData);
	if (!succeeded) {
		return false;
	}

	outLevelData = std::move(levelData);
	return true;
}

}
