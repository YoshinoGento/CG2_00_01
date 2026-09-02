#include "application/magnet/stage/MagnetStageSystem.h"

#include "io/JsonFile.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <filesystem>
#include <limits>
#include <random>
#include <system_error>
#include <utility>

namespace magnet {
namespace {

constexpr char kSchemaName[] = "magnet_stage";
constexpr char kPlayerType[] = "player";
constexpr char kMagnetBallType[] = "magnet_ball";
constexpr char kGoalType[] = "goal";
constexpr char kObstacleType[] = "obstacle";
constexpr float kBallPlaneHeight = 0.5f;
constexpr float kPlayerPlaneHeight = 0.75f;
constexpr float kMinimumStageExtent = 2.0f;
constexpr float kMaximumAbsoluteCoordinate = 100.0f;
constexpr float kMinimumAllowedSpacing = 0.5f;
constexpr float kMaximumAllowedSpacing = 20.0f;
constexpr float kMaximumPlayerClearRadius = 30.0f;
constexpr float kMinimumBoxSize = 0.10f;
constexpr float kMaximumBoxSize = 50.0f;
constexpr float kMaximumBoxHeight = 50.0f;
constexpr std::size_t kCandidateCountPerBall = 96;
constexpr std::size_t kMaximumStageNameLength = 64;
constexpr std::size_t kMaximumPathLength = 260;

float DistanceSquaredXZ(const Vector3& a, const Vector3& b) noexcept
{
	const float deltaX = a.x - b.x;
	const float deltaZ = a.z - b.z;
	return deltaX * deltaX + deltaZ * deltaZ;
}

bool ReadFiniteFloat(const nlohmann::json& object, const char* key, float& output)
{
	if (!object.contains(key) || !object[key].is_number()) {
		return false;
	}
	output = object[key].get<float>();
	return std::isfinite(output);
}

bool ReadPosition(const nlohmann::json& object, Vector3& output)
{
	if (!object.contains("position") || !object["position"].is_array() ||
		object["position"].size() != 3) {
		return false;
	}
	for (std::size_t component = 0; component < 3; ++component) {
		if (!object["position"][component].is_number()) {
			return false;
		}
	}
	output = {
		object["position"][0].get<float>(),
		object["position"][1].get<float>(),
		object["position"][2].get<float>(),
	};
	return std::isfinite(output.x) && std::isfinite(output.y) &&
		std::isfinite(output.z);
}

bool ReadSize(const nlohmann::json& object, Vector3& output)
{
	if (!object.contains("size") || !object["size"].is_array() ||
		object["size"].size() != 3) {
		return false;
	}
	for (std::size_t component = 0; component < 3; ++component) {
		if (!object["size"][component].is_number()) {
			return false;
		}
	}
	output = {
		object["size"][0].get<float>(),
		object["size"][1].get<float>(),
		object["size"][2].get<float>(),
	};
	return std::isfinite(output.x) && std::isfinite(output.y) &&
		std::isfinite(output.z);
}

} // namespace

MagnetStageSystem::MagnetStageSystem(std::string saveDirectory)
	: saveDirectory_(std::move(saveDirectory))
{
}

bool MagnetStageSystem::Initialize()
{
	std::string defaultStagePath;
	if (BuildSavePath("stage_01", defaultStagePath) && Load(defaultStagePath)) {
		(void)RefreshSaveEntriesInternal();
		return true;
	}

	MagnetStageGenerationSettings defaultSettings{};
	if (!GenerateBalanced(defaultSettings)) {
		SetOperationResult(false, "初期ステージの生成に失敗しました。");
		return false;
	}
	SetOperationResult(
		true,
		"初期JSONを読み込めなかったため、偏りを抑えたステージを生成しました。");
	(void)RefreshSaveEntriesInternal();
	return true;
}

bool MagnetStageSystem::GenerateBalanced(
	const MagnetStageGenerationSettings& settings)
{
	if (!ValidateGenerationSettings(settings)) {
		SetOperationResult(false, "ランダム配置の設定が不正です。");
		return false;
	}

	MagnetStageData candidate{};
	candidate.name = stageData_.name.empty() ? "stage_01" : stageData_.name;
	candidate.generation = settings;
	candidate.playerPosition = stageData_.playerPosition;
	candidate.ballCount = settings.ballCount;
	candidate.goals = stageData_.goals;
	candidate.goalCount = stageData_.goalCount;
	candidate.obstacles = stageData_.obstacles;
	candidate.obstacleCount = stageData_.obstacleCount;

	std::mt19937 randomEngine(settings.seed);
	std::array<uint32_t, MagnetStageData::kMaximumBallCount> quadrants{};
	for (std::size_t index = 0; index < settings.ballCount; ++index) {
		quadrants[index] = static_cast<uint32_t>(index % 4);
	}
	std::shuffle(
		quadrants.begin(),
		quadrants.begin() + static_cast<std::ptrdiff_t>(settings.ballCount),
		randomEngine);

	const float centerX = (settings.minimumX + settings.maximumX) * 0.5f;
	const float centerZ = (settings.minimumZ + settings.maximumZ) * 0.5f;
	const Vector3 playerPosition = candidate.playerPosition;
	const float minimumSpacingSquared = settings.minimumSpacing * settings.minimumSpacing;
	const float playerClearRadiusSquared =
		settings.playerClearRadius * settings.playerClearRadius;

	for (std::size_t ballIndex = 0; ballIndex < settings.ballCount; ++ballIndex) {
		const uint32_t quadrant = quadrants[ballIndex];
		const bool positiveX = (quadrant & 1u) != 0;
		const bool positiveZ = (quadrant & 2u) != 0;
		const float quadrantMinimumX = positiveX ? centerX : settings.minimumX;
		const float quadrantMaximumX = positiveX ? settings.maximumX : centerX;
		const float quadrantMinimumZ = positiveZ ? centerZ : settings.minimumZ;
		const float quadrantMaximumZ = positiveZ ? settings.maximumZ : centerZ;
		std::uniform_real_distribution<float> xDistribution(
			quadrantMinimumX,
			quadrantMaximumX);
		std::uniform_real_distribution<float> zDistribution(
			quadrantMinimumZ,
			quadrantMaximumZ);

		Vector3 bestPosition{};
		float bestMinimumDistanceSquared = -1.0f;
		for (std::size_t candidateIndex = 0;
			candidateIndex < kCandidateCountPerBall;
			++candidateIndex) {
			const Vector3 position{
				xDistribution(randomEngine),
				kBallPlaneHeight,
				zDistribution(randomEngine),
			};
			float candidateMinimumDistanceSquared =
				DistanceSquaredXZ(position, playerPosition);
			for (std::size_t existingIndex = 0; existingIndex < ballIndex; ++existingIndex) {
				candidateMinimumDistanceSquared = (std::min)(
					candidateMinimumDistanceSquared,
					DistanceSquaredXZ(position, candidate.balls[existingIndex].position));
			}
			if (candidateMinimumDistanceSquared > bestMinimumDistanceSquared) {
				bestMinimumDistanceSquared = candidateMinimumDistanceSquared;
				bestPosition = position;
			}
		}

		if (!IsFinitePosition(bestPosition) ||
			bestMinimumDistanceSquared < minimumSpacingSquared ||
			DistanceSquaredXZ(bestPosition, playerPosition) < playerClearRadiusSquared) {
			SetOperationResult(
				false,
				"指定した球数と間隔ではステージ範囲内に配置できません。");
			return false;
		}
		candidate.balls[ballIndex] = {
			static_cast<uint32_t>(ballIndex + 1),
			bestPosition,
		};
	}

	if (!ValidateStageData(candidate)) {
		SetOperationResult(false, "生成したステージの検証に失敗しました。");
		return false;
	}
	stageData_ = std::move(candidate);
	dirty_ = true;
	SetOperationResult(true, "偏りを抑えて磁石球をランダム配置しました。");
	return true;
}

bool MagnetStageSystem::AddBall(const Vector3& position)
{
	if (stageData_.ballCount >= stageData_.balls.size() ||
		!IsFinitePosition(position) ||
		position.x < stageData_.generation.minimumX ||
		position.x > stageData_.generation.maximumX ||
		position.z < stageData_.generation.minimumZ ||
		position.z > stageData_.generation.maximumZ) {
		SetOperationResult(false, "球の位置が不正か、配置上限に達しています。");
		return false;
	}

	uint32_t maximumId = 0;
	for (std::size_t index = 0; index < stageData_.ballCount; ++index) {
		maximumId = (std::max)(maximumId, stageData_.balls[index].id);
	}
	if (maximumId == (std::numeric_limits<uint32_t>::max)()) {
		SetOperationResult(false, "新しい球に割り当てられるIDがありません。");
		return false;
	}
	Vector3 planarPosition = position;
	planarPosition.y = kBallPlaneHeight;
	stageData_.balls[stageData_.ballCount++] = { maximumId + 1, planarPosition };
	dirty_ = true;
	SetOperationResult(true, "磁石球を追加しました。");
	return true;
}

bool MagnetStageSystem::RemoveBall(uint32_t id)
{
	for (std::size_t index = 0; index < stageData_.ballCount; ++index) {
		if (stageData_.balls[index].id != id) {
			continue;
		}
		for (std::size_t moveIndex = index + 1;
			moveIndex < stageData_.ballCount;
			++moveIndex) {
			stageData_.balls[moveIndex - 1] = stageData_.balls[moveIndex];
		}
		--stageData_.ballCount;
		stageData_.balls[stageData_.ballCount] = {};
		dirty_ = true;
		SetOperationResult(true, "選択した磁石球を削除しました。");
		return true;
	}
	SetOperationResult(false, "選択した磁石球が見つかりません。");
	return false;
}

bool MagnetStageSystem::SetBallPosition(uint32_t id, const Vector3& position)
{
	if (!IsFinitePosition(position) ||
		position.x < stageData_.generation.minimumX ||
		position.x > stageData_.generation.maximumX ||
		position.z < stageData_.generation.minimumZ ||
		position.z > stageData_.generation.maximumZ) {
		SetOperationResult(false, "編集した球の位置がステージ範囲外です。");
		return false;
	}
	for (std::size_t index = 0; index < stageData_.ballCount; ++index) {
		if (stageData_.balls[index].id == id) {
			stageData_.balls[index].position = {
				position.x,
				kBallPlaneHeight,
				position.z,
			};
			dirty_ = true;
			SetOperationResult(true, "選択した磁石球の位置を更新しました。");
			return true;
		}
	}
	SetOperationResult(false, "選択した磁石球が見つかりません。");
	return false;
}

bool MagnetStageSystem::SetPlayerPosition(const Vector3& position)
{
	if (!IsFinitePosition(position) ||
		position.x < stageData_.generation.minimumX ||
		position.x > stageData_.generation.maximumX ||
		position.z < stageData_.generation.minimumZ ||
		position.z > stageData_.generation.maximumZ) {
		SetOperationResult(false, "プレイヤーの開始位置がステージ範囲外です。");
		return false;
	}
	stageData_.playerPosition = {
		position.x,
		kPlayerPlaneHeight,
		position.z,
	};
	dirty_ = true;
	SetOperationResult(true, "プレイヤーの開始位置を更新しました。");
	return true;
}

bool MagnetStageSystem::AddBoxObject(
	MagnetStageObjectType type,
	const Vector3& position,
	const Vector3& size)
{
	MagnetStageBoxPlacement placement{};
	placement.position = position;
	placement.size = size;
	if (!IsValidBoxPlacement(placement) ||
		position.x < stageData_.generation.minimumX ||
		position.x > stageData_.generation.maximumX ||
		position.z < stageData_.generation.minimumZ ||
		position.z > stageData_.generation.maximumZ) {
		SetOperationResult(false, "配置するオブジェクトの位置またはサイズが不正です。");
		return false;
	}

	const auto addToArray = [&](auto& placements, std::size_t& count) {
		if (count >= placements.size()) {
			return false;
		}
		uint32_t maximumId = 0;
		for (std::size_t index = 0; index < count; ++index) {
			maximumId = (std::max)(maximumId, placements[index].id);
		}
		if (maximumId == (std::numeric_limits<uint32_t>::max)()) {
			return false;
		}
		placement.id = maximumId + 1;
		placements[count++] = placement;
		return true;
	};

	bool added = false;
	if (type == MagnetStageObjectType::Goal) {
		added = addToArray(stageData_.goals, stageData_.goalCount);
	} else if (type == MagnetStageObjectType::Obstacle) {
		added = addToArray(stageData_.obstacles, stageData_.obstacleCount);
	}
	if (!added) {
		SetOperationResult(false, "オブジェクトの上限に達しているか種類が不正です。");
		return false;
	}
	dirty_ = true;
	SetOperationResult(true, "ステージオブジェクトを追加しました。");
	return true;
}

bool MagnetStageSystem::RemoveBoxObject(MagnetStageObjectType type, uint32_t id)
{
	const auto removeFromArray = [&](auto& placements, std::size_t& count) {
		for (std::size_t index = 0; index < count; ++index) {
			if (placements[index].id != id) {
				continue;
			}
			for (std::size_t moveIndex = index + 1; moveIndex < count; ++moveIndex) {
				placements[moveIndex - 1] = placements[moveIndex];
			}
			--count;
			placements[count] = {};
			return true;
		}
		return false;
	};

	bool removed = false;
	if (type == MagnetStageObjectType::Goal) {
		removed = removeFromArray(stageData_.goals, stageData_.goalCount);
	} else if (type == MagnetStageObjectType::Obstacle) {
		removed = removeFromArray(stageData_.obstacles, stageData_.obstacleCount);
	}
	if (!removed) {
		SetOperationResult(false, "選択したステージオブジェクトが見つかりません。");
		return false;
	}
	dirty_ = true;
	SetOperationResult(true, "選択したステージオブジェクトを削除しました。");
	return true;
}

bool MagnetStageSystem::SetBoxObjectTransform(
	MagnetStageObjectType type,
	uint32_t id,
	const Vector3& position,
	const Vector3& size)
{
	MagnetStageBoxPlacement candidate{ id, position, size };
	if (!IsValidBoxPlacement(candidate) ||
		position.x < stageData_.generation.minimumX ||
		position.x > stageData_.generation.maximumX ||
		position.z < stageData_.generation.minimumZ ||
		position.z > stageData_.generation.maximumZ) {
		SetOperationResult(false, "編集した位置またはサイズがステージ範囲外です。");
		return false;
	}
	const auto updateArray = [&](auto& placements, std::size_t count) {
		for (std::size_t index = 0; index < count; ++index) {
			if (placements[index].id == id) {
				candidate.score = placements[index].score;
				placements[index] = candidate;
				return true;
			}
		}
		return false;
	};
	const bool updated = type == MagnetStageObjectType::Goal
		? updateArray(stageData_.goals, stageData_.goalCount)
		: type == MagnetStageObjectType::Obstacle
			? updateArray(stageData_.obstacles, stageData_.obstacleCount)
			: false;
	if (!updated) {
		SetOperationResult(false, "選択したステージオブジェクトが見つかりません。");
		return false;
	}
	dirty_ = true;
	SetOperationResult(true, "ステージオブジェクトの配置を更新しました。");
	return true;
}

bool MagnetStageSystem::SetGoalScore(uint32_t id, uint32_t score)
{
	if (score == 0 || score > 999) {
		SetOperationResult(false, "ゴール得点は1～999の範囲で指定してください。");
		return false;
	}
	for (std::size_t index = 0; index < stageData_.goalCount; ++index) {
		if (stageData_.goals[index].id == id) {
			stageData_.goals[index].score = score;
			dirty_ = true;
			SetOperationResult(true, "選択したゴールの得点を更新しました。");
			return true;
		}
	}
	SetOperationResult(false, "選択したゴールが見つかりません。");
	return false;
}

bool MagnetStageSystem::Save(const std::string& path)
{
	try {
		if (!IsSafeJsonPath(path) || !ValidateStageData(stageData_)) {
			SetOperationResult(false, "ステージの保存先またはデータが不正です。");
			return false;
		}

		nlohmann::json objects = nlohmann::json::array();
		objects.push_back({
			{ "id", 1u },
			{ "type", kPlayerType },
			{ "position", {
				stageData_.playerPosition.x,
				stageData_.playerPosition.y,
				stageData_.playerPosition.z } },
		});
		for (std::size_t index = 0; index < stageData_.ballCount; ++index) {
			const MagnetStageBallPlacement& ball = stageData_.balls[index];
			objects.push_back({
				{ "id", ball.id },
				{ "type", kMagnetBallType },
				{ "position", { ball.position.x, ball.position.y, ball.position.z } },
			});
		}
		const auto appendBoxes = [&](const auto& placements, std::size_t count, const char* type) {
			for (std::size_t index = 0; index < count; ++index) {
				const MagnetStageBoxPlacement& placement = placements[index];
				nlohmann::json object = {
					{ "id", placement.id },
					{ "type", type },
					{ "position", {
						placement.position.x,
						placement.position.y,
						placement.position.z } },
					{ "size", { placement.size.x, placement.size.y, placement.size.z } },
				};
				if (std::strcmp(type, kGoalType) == 0) {
					object["score"] = placement.score;
				}
				objects.push_back(std::move(object));
			}
		};
		appendBoxes(stageData_.goals, stageData_.goalCount, kGoalType);
		appendBoxes(stageData_.obstacles, stageData_.obstacleCount, kObstacleType);

		const MagnetStageGenerationSettings& settings = stageData_.generation;
		nlohmann::json root = {
			{ "schema", kSchemaName },
			{ "schemaVersion", MagnetStageData::kSchemaVersion },
			{ "name", stageData_.name },
			{ "bounds", {
				{ "minimumX", settings.minimumX },
				{ "maximumX", settings.maximumX },
				{ "minimumZ", settings.minimumZ },
				{ "maximumZ", settings.maximumZ },
			} },
			{ "generator", {
				{ "seed", settings.seed },
				{ "minimumSpacing", settings.minimumSpacing },
				{ "playerClearRadius", settings.playerClearRadius },
			} },
			{ "objects", std::move(objects) },
		};

		std::error_code error;
		const std::filesystem::path parent = std::filesystem::path(path).parent_path();
		if (!parent.empty()) {
			std::filesystem::create_directories(parent, error);
			if (error) {
				SetOperationResult(false, "ステージ保存フォルダーを作成できませんでした。");
				return false;
			}
		}
		if (!JsonFile::Save(path, root)) {
			SetOperationResult(false, "ステージJSONの保存に失敗しました。");
			return false;
		}
		dirty_ = false;
		SetOperationResult(true, "ステージJSONを保存しました。");
		return true;
	}
	catch (const std::exception&) {
		SetOperationResult(false, "保存先または値が不正なため、ステージJSONを保存できませんでした。");
		return false;
	}
}

bool MagnetStageSystem::Load(const std::string& path)
{
	try {
		if (!IsSafeJsonPath(path)) {
			SetOperationResult(false, "ステージの読込先が不正です。");
			return false;
		}

		nlohmann::json root;
		if (!JsonFile::Load(path, root)) {
			SetOperationResult(false, "ステージJSONの読込に失敗しました。");
			return false;
		}
		if (!root.is_object() || !root.contains("schema") ||
			!root["schema"].is_string() || root["schema"].get<std::string>() != kSchemaName ||
			!root.contains("schemaVersion") || !root["schemaVersion"].is_number_unsigned() ||
			!root.contains("name") || !root["name"].is_string() ||
			!root.contains("bounds") || !root["bounds"].is_object() ||
			!root.contains("generator") || !root["generator"].is_object() ||
			!root.contains("objects") || !root["objects"].is_array() ||
			root["objects"].size() >
				1u + MagnetStageData::kMaximumBallCount +
				MagnetStageData::kMaximumGoalCount +
				MagnetStageData::kMaximumObstacleCount) {
			SetOperationResult(false, "ステージJSONの形式が不正か未対応です。");
			return false;
		}
		const uint32_t schemaVersion = root["schemaVersion"].get<uint32_t>();
		if (schemaVersion < MagnetStageData::kOldestSupportedSchemaVersion ||
			schemaVersion > MagnetStageData::kSchemaVersion) {
			SetOperationResult(false, "このステージJSONのバージョンには対応していません。");
			return false;
		}

		MagnetStageData candidate{};
		candidate.name = root["name"].get<std::string>();
		const nlohmann::json& bounds = root["bounds"];
		const nlohmann::json& generator = root["generator"];
		if (!ReadFiniteFloat(bounds, "minimumX", candidate.generation.minimumX) ||
			!ReadFiniteFloat(bounds, "maximumX", candidate.generation.maximumX) ||
			!ReadFiniteFloat(bounds, "minimumZ", candidate.generation.minimumZ) ||
			!ReadFiniteFloat(bounds, "maximumZ", candidate.generation.maximumZ) ||
			!generator.contains("seed") || !generator["seed"].is_number_unsigned() ||
			!ReadFiniteFloat(generator, "minimumSpacing", candidate.generation.minimumSpacing) ||
			!ReadFiniteFloat(generator, "playerClearRadius", candidate.generation.playerClearRadius)) {
			SetOperationResult(false, "ステージ範囲または生成設定が不正です。");
			return false;
		}
		candidate.generation.seed = generator["seed"].get<uint32_t>();

		bool playerFound = false;
		for (std::size_t index = 0; index < root["objects"].size(); ++index) {
			const nlohmann::json& object = root["objects"][index];
			if (!object.is_object() || !object.contains("id") ||
				!object["id"].is_number_unsigned() || !object.contains("type") ||
				!object["type"].is_string()) {
				SetOperationResult(false, "ステージに未対応のオブジェクトがあります。");
				return false;
			}
			Vector3 position{};
			if (!ReadPosition(object, position)) {
				SetOperationResult(false, "ステージオブジェクトの位置が不正です。");
				return false;
			}
			const uint32_t id = object["id"].get<uint32_t>();
			const std::string type = object["type"].get<std::string>();
			if (type == kPlayerType) {
				if (schemaVersion < 3u || playerFound || id != 1u) {
					SetOperationResult(false, "プレイヤー配置が重複しているかIDが不正です。");
					return false;
				}
				position.y = kPlayerPlaneHeight;
				candidate.playerPosition = position;
				playerFound = true;
				continue;
			}
			if (type == kMagnetBallType) {
				if (candidate.ballCount >= candidate.balls.size()) {
					SetOperationResult(false, "ステージ内の磁石球が上限を超えています。");
					return false;
				}
				position.y = kBallPlaneHeight;
				candidate.balls[candidate.ballCount++] = { id, position };
				continue;
			}

			Vector3 size{};
			if (!ReadSize(object, size)) {
				SetOperationResult(false, "ゴールまたは障害物のサイズが不正です。");
				return false;
			}
			if (type == kGoalType && candidate.goalCount < candidate.goals.size()) {
				uint32_t score = 1;
				if (object.contains("score")) {
					if (!object["score"].is_number_unsigned()) {
						SetOperationResult(false, "ゴール得点が不正です。");
						return false;
					}
					score = object["score"].get<uint32_t>();
				}
				candidate.goals[candidate.goalCount++] = { id, position, size, score };
			} else if (type == kObstacleType &&
				candidate.obstacleCount < candidate.obstacles.size()) {
				candidate.obstacles[candidate.obstacleCount++] = { id, position, size };
			} else {
				SetOperationResult(false, "未対応のオブジェクトがあるか、配置上限を超えています。");
				return false;
			}
		}
		if (schemaVersion >= 3u && !playerFound) {
			SetOperationResult(false, "プレイヤーの開始位置がステージJSONにありません。");
			return false;
		}
		candidate.generation.ballCount = candidate.ballCount;

		if (!ValidateStageData(candidate)) {
			SetOperationResult(false, "読み込んだステージの検証に失敗しました。");
			return false;
		}
		stageData_ = std::move(candidate);
		dirty_ = false;
		SetOperationResult(true, "ステージJSONを読み込みました。");
		return true;
	}
	catch (const std::exception&) {
		SetOperationResult(false, "読込先または値が不正なため、ステージJSONを読み込めませんでした。");
		return false;
	}
}

bool MagnetStageSystem::RefreshSaveEntries()
{
	const bool refreshed = RefreshSaveEntriesInternal();
	SetOperationResult(
		refreshed,
		refreshed
			? "ステージのセーブ一覧を更新しました。"
			: "ステージのセーブ一覧を更新できませんでした。");
	return refreshed;
}

bool MagnetStageSystem::SaveNamed(
	const std::string& saveName,
	bool allowOverwrite)
{
	try {
		std::string savePath;
		if (!BuildSavePath(saveName, savePath)) {
			SetOperationResult(
				false,
				"セーブ名は1～48文字の半角英数字、_、-で入力してください。");
			return false;
		}

		std::error_code error;
		const bool alreadyExists = std::filesystem::exists(savePath, error);
		if (error) {
			SetOperationResult(false, "同名のセーブデータを確認できませんでした。");
			return false;
		}
		if (alreadyExists && !allowOverwrite) {
			SetOperationResult(false, "同名のセーブデータがあります。上書きを確認してください。");
			return false;
		}

		const std::string previousName = stageData_.name;
		const bool previousDirty = dirty_;
		stageData_.name = saveName;
		if (!Save(savePath)) {
			stageData_.name = previousName;
			dirty_ = previousDirty;
			return false;
		}

		const bool listRefreshed = RefreshSaveEntriesInternal();
		SetOperationResult(
			true,
			listRefreshed
				? "名前を付けてステージを保存しました。"
				: "ステージは保存しましたが、セーブ一覧を更新できませんでした。");
		return true;
	}
	catch (const std::exception&) {
		SetOperationResult(false, "保存先または値が不正なため、名前付きセーブに失敗しました。");
		return false;
	}
}

bool MagnetStageSystem::LoadNamed(const std::string& saveName)
{
	std::string savePath;
	if (!BuildSavePath(saveName, savePath)) {
		SetOperationResult(false, "選択したセーブ名が不正です。");
		return false;
	}
	return Load(savePath);
}

const MagnetStageBallPlacement* MagnetStageSystem::FindBall(uint32_t id) const noexcept
{
	for (std::size_t index = 0; index < stageData_.ballCount; ++index) {
		if (stageData_.balls[index].id == id) {
			return &stageData_.balls[index];
		}
	}
	return nullptr;
}

const MagnetStageBoxPlacement* MagnetStageSystem::FindBoxObject(
	MagnetStageObjectType type,
	uint32_t id) const noexcept
{
	const auto findInArray = [&](const auto& placements, std::size_t count)
		-> const MagnetStageBoxPlacement* {
		for (std::size_t index = 0; index < count; ++index) {
			if (placements[index].id == id) {
				return &placements[index];
			}
		}
		return nullptr;
	};
	if (type == MagnetStageObjectType::Goal) {
		return findInArray(stageData_.goals, stageData_.goalCount);
	}
	if (type == MagnetStageObjectType::Obstacle) {
		return findInArray(stageData_.obstacles, stageData_.obstacleCount);
	}
	return nullptr;
}

bool MagnetStageSystem::IsFinitePosition(const Vector3& position) noexcept
{
	return std::isfinite(position.x) && std::isfinite(position.y) &&
		std::isfinite(position.z) &&
		std::abs(position.x) <= kMaximumAbsoluteCoordinate &&
		std::abs(position.z) <= kMaximumAbsoluteCoordinate;
}

bool MagnetStageSystem::IsValidBoxPlacement(
	const MagnetStageBoxPlacement& placement) noexcept
{
	return placement.id != (std::numeric_limits<uint32_t>::max)() &&
		IsFinitePosition(placement.position) &&
		std::isfinite(placement.position.y) &&
		placement.position.y >= 0.0f && placement.position.y <= kMaximumBoxHeight &&
		std::isfinite(placement.size.x) && std::isfinite(placement.size.y) &&
		std::isfinite(placement.size.z) &&
		placement.size.x >= kMinimumBoxSize && placement.size.x <= kMaximumBoxSize &&
		placement.size.y >= kMinimumBoxSize && placement.size.y <= kMaximumBoxSize &&
		placement.size.z >= kMinimumBoxSize && placement.size.z <= kMaximumBoxSize;
}

bool MagnetStageSystem::IsSafeJsonPath(const std::string& path)
{
	if (path.empty() || path.size() > kMaximumPathLength) {
		return false;
	}
	const std::filesystem::path normalized = std::filesystem::path(path).lexically_normal();
	if (normalized.empty() || normalized.is_absolute() || normalized.has_root_name() ||
		normalized.has_root_directory() || normalized.extension() != ".json") {
		return false;
	}
	for (const std::filesystem::path& component : normalized) {
		if (component == "..") {
			return false;
		}
	}
	return true;
}

bool MagnetStageSystem::IsSafeSaveName(const std::string& saveName) noexcept
{
	if (saveName.empty() || saveName.size() > kMaximumSaveNameLength) {
		return false;
	}
	for (const unsigned char character : saveName) {
		const bool alphaNumeric =
			(character >= 'a' && character <= 'z') ||
			(character >= 'A' && character <= 'Z') ||
			(character >= '0' && character <= '9');
		if (!alphaNumeric && character != '_' && character != '-') {
			return false;
		}
	}

	std::array<char, MagnetStageSystem::kMaximumSaveNameLength + 1> lowerName{};
	for (std::size_t index = 0; index < saveName.size(); ++index) {
		const char character = saveName[index];
		lowerName[index] = character >= 'A' && character <= 'Z'
			? static_cast<char>(character - 'A' + 'a')
			: character;
	}
	const std::string normalizedName(lowerName.data(), saveName.size());
	if (normalizedName == "con" || normalizedName == "prn" ||
		normalizedName == "aux" || normalizedName == "nul") {
		return false;
	}
	if (normalizedName.size() == 4 &&
		(normalizedName.rfind("com", 0) == 0 || normalizedName.rfind("lpt", 0) == 0) &&
		normalizedName[3] >= '1' && normalizedName[3] <= '9') {
		return false;
	}
	return true;
}

bool MagnetStageSystem::ValidateGenerationSettings(
	const MagnetStageGenerationSettings& settings) noexcept
{
	return settings.ballCount > 0 &&
		settings.ballCount <= MagnetStageData::kMaximumBallCount &&
		std::isfinite(settings.minimumX) && std::isfinite(settings.maximumX) &&
		std::isfinite(settings.minimumZ) && std::isfinite(settings.maximumZ) &&
		std::isfinite(settings.minimumSpacing) &&
		std::isfinite(settings.playerClearRadius) &&
		settings.minimumX >= -kMaximumAbsoluteCoordinate &&
		settings.maximumX <= kMaximumAbsoluteCoordinate &&
		settings.minimumZ >= -kMaximumAbsoluteCoordinate &&
		settings.maximumZ <= kMaximumAbsoluteCoordinate &&
		settings.maximumX - settings.minimumX >= kMinimumStageExtent &&
		settings.maximumZ - settings.minimumZ >= kMinimumStageExtent &&
		settings.minimumSpacing >= kMinimumAllowedSpacing &&
		settings.minimumSpacing <= kMaximumAllowedSpacing &&
		settings.playerClearRadius >= 0.0f &&
		settings.playerClearRadius <= kMaximumPlayerClearRadius;
}

bool MagnetStageSystem::ValidateStageData(const MagnetStageData& stageData) noexcept
{
	if (stageData.name.empty() || stageData.name.size() > kMaximumStageNameLength ||
		stageData.ballCount > stageData.balls.size() ||
		stageData.goalCount > stageData.goals.size() ||
		stageData.obstacleCount > stageData.obstacles.size()) {
		return false;
	}
	MagnetStageGenerationSettings validationSettings = stageData.generation;
	validationSettings.ballCount = (std::max)(stageData.ballCount, std::size_t{ 1 });
	if (!ValidateGenerationSettings(validationSettings)) {
		return false;
	}
	if (!IsFinitePosition(stageData.playerPosition) ||
		std::abs(stageData.playerPosition.y - kPlayerPlaneHeight) > 1.0e-4f ||
		stageData.playerPosition.x < stageData.generation.minimumX ||
		stageData.playerPosition.x > stageData.generation.maximumX ||
		stageData.playerPosition.z < stageData.generation.minimumZ ||
		stageData.playerPosition.z > stageData.generation.maximumZ) {
		return false;
	}
	for (std::size_t index = 0; index < stageData.ballCount; ++index) {
		const MagnetStageBallPlacement& ball = stageData.balls[index];
		if (ball.id == 0 || !IsFinitePosition(ball.position) ||
			ball.position.x < stageData.generation.minimumX ||
			ball.position.x > stageData.generation.maximumX ||
			ball.position.z < stageData.generation.minimumZ ||
			ball.position.z > stageData.generation.maximumZ) {
			return false;
		}
		for (std::size_t previousIndex = 0; previousIndex < index; ++previousIndex) {
			if (stageData.balls[previousIndex].id == ball.id) {
				return false;
			}
		}
	}
	const auto validateBoxes = [&](const auto& placements, std::size_t count) {
		for (std::size_t index = 0; index < count; ++index) {
			const MagnetStageBoxPlacement& placement = placements[index];
			if (placement.id == 0 || !IsValidBoxPlacement(placement) ||
				placement.position.x < stageData.generation.minimumX ||
				placement.position.x > stageData.generation.maximumX ||
				placement.position.z < stageData.generation.minimumZ ||
				placement.position.z > stageData.generation.maximumZ) {
				return false;
			}
			for (std::size_t previousIndex = 0; previousIndex < index; ++previousIndex) {
				if (placements[previousIndex].id == placement.id) {
					return false;
				}
			}
		}
		return true;
	};
	if (!validateBoxes(stageData.goals, stageData.goalCount) ||
		!validateBoxes(stageData.obstacles, stageData.obstacleCount)) {
		return false;
	}
	for (std::size_t index = 0; index < stageData.goalCount; ++index) {
		if (stageData.goals[index].score == 0 || stageData.goals[index].score > 999) {
			return false;
		}
	}
	return true;
}

bool MagnetStageSystem::BuildSavePath(
	const std::string& saveName,
	std::string& outputPath) const
{
	if (!IsSafeSaveName(saveName)) {
		return false;
	}
	try {
		const std::filesystem::path path =
			std::filesystem::path(saveDirectory_) / (saveName + ".json");
		outputPath = path.lexically_normal().generic_string();
		return IsSafeJsonPath(outputPath);
	}
	catch (const std::exception&) {
		outputPath.clear();
		return false;
	}
}

bool MagnetStageSystem::RefreshSaveEntriesInternal() noexcept
{
	try {
		std::array<MagnetStageSaveEntry, kMaximumSaveEntryCount> candidates{};
		std::size_t candidateCount = 0;
		std::error_code error;
		const std::filesystem::path directory(saveDirectory_);
		const bool directoryExists = std::filesystem::exists(directory, error);
		if (error) {
			return false;
		}
		if (!directoryExists) {
			saveEntries_.fill({});
			saveEntryCount_ = 0;
			return true;
		}
		if (!std::filesystem::is_directory(directory, error) || error) {
			return false;
		}

		std::filesystem::directory_iterator iterator(directory, error);
		const std::filesystem::directory_iterator end;
		while (!error && iterator != end) {
			std::error_code entryError;
			if (iterator->is_regular_file(entryError) && !entryError &&
				iterator->path().extension() == ".json") {
				const std::string saveName = iterator->path().stem().string();
				const std::string relativePath =
					iterator->path().lexically_normal().generic_string();
				if (IsSafeSaveName(saveName) && IsSafeJsonPath(relativePath)) {
					MagnetStageSaveEntry candidate{ saveName, relativePath };
					if (candidateCount < candidates.size()) {
						candidates[candidateCount++] = std::move(candidate);
					} else {
						auto largest = std::max_element(
							candidates.begin(),
							candidates.end(),
							[](const MagnetStageSaveEntry& left, const MagnetStageSaveEntry& right) {
								return left.name < right.name;
							});
						if (largest != candidates.end() && candidate.name < largest->name) {
							*largest = std::move(candidate);
						}
					}
				}
			}
			iterator.increment(error);
		}
		if (error) {
			return false;
		}

		std::sort(
			candidates.begin(),
			candidates.begin() + static_cast<std::ptrdiff_t>(candidateCount),
			[](const MagnetStageSaveEntry& left, const MagnetStageSaveEntry& right) {
				return left.name < right.name;
			});
		saveEntries_ = std::move(candidates);
		saveEntryCount_ = candidateCount;
		return true;
	}
	catch (const std::exception&) {
		return false;
	}
}

void MagnetStageSystem::SetOperationResult(
	bool succeeded,
	const std::string& message)
{
	lastOperationSucceeded_ = succeeded;
	lastOperationMessage_ = message;
}

} // namespace magnet
