#include "application/magnet/stage/MagnetStageSystem.h"

#include "io/JsonFile.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <limits>
#include <random>
#include <system_error>
#include <utility>

namespace magnet {
namespace {

constexpr char kSchemaName[] = "magnet_stage";
constexpr char kMagnetBallType[] = "magnet_ball";
constexpr float kBallPlaneHeight = 0.5f;
constexpr float kMinimumStageExtent = 2.0f;
constexpr float kMaximumAbsoluteCoordinate = 100.0f;
constexpr float kMinimumAllowedSpacing = 0.5f;
constexpr float kMaximumAllowedSpacing = 20.0f;
constexpr float kMaximumPlayerClearRadius = 30.0f;
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
		SetOperationResult(false, "Default stage generation failed.");
		return false;
	}
	SetOperationResult(
		true,
		"Default JSON was unavailable; generated an in-memory balanced stage.");
	(void)RefreshSaveEntriesInternal();
	return true;
}

bool MagnetStageSystem::GenerateBalanced(
	const MagnetStageGenerationSettings& settings)
{
	if (!ValidateGenerationSettings(settings)) {
		SetOperationResult(false, "Random layout settings are invalid.");
		return false;
	}

	MagnetStageData candidate{};
	candidate.name = stageData_.name.empty() ? "stage_01" : stageData_.name;
	candidate.generation = settings;
	candidate.ballCount = settings.ballCount;

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
	const Vector3 playerPosition{ 0.0f, kBallPlaneHeight, 0.0f };
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
				"The requested count/spacing does not fit inside the stage bounds.");
			return false;
		}
		candidate.balls[ballIndex] = {
			static_cast<uint32_t>(ballIndex + 1),
			bestPosition,
		};
	}

	if (!ValidateStageData(candidate)) {
		SetOperationResult(false, "Generated stage validation failed.");
		return false;
	}
	stageData_ = std::move(candidate);
	dirty_ = true;
	SetOperationResult(true, "Generated a balanced random magnet-ball layout.");
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
		SetOperationResult(false, "Ball position is invalid or the stage is full.");
		return false;
	}

	uint32_t maximumId = 0;
	for (std::size_t index = 0; index < stageData_.ballCount; ++index) {
		maximumId = (std::max)(maximumId, stageData_.balls[index].id);
	}
	if (maximumId == (std::numeric_limits<uint32_t>::max)()) {
		SetOperationResult(false, "No stage-ball IDs remain.");
		return false;
	}
	Vector3 planarPosition = position;
	planarPosition.y = kBallPlaneHeight;
	stageData_.balls[stageData_.ballCount++] = { maximumId + 1, planarPosition };
	dirty_ = true;
	SetOperationResult(true, "Added a magnet ball.");
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
		SetOperationResult(true, "Removed the selected magnet ball.");
		return true;
	}
	SetOperationResult(false, "The selected magnet-ball ID does not exist.");
	return false;
}

bool MagnetStageSystem::SetBallPosition(uint32_t id, const Vector3& position)
{
	if (!IsFinitePosition(position) ||
		position.x < stageData_.generation.minimumX ||
		position.x > stageData_.generation.maximumX ||
		position.z < stageData_.generation.minimumZ ||
		position.z > stageData_.generation.maximumZ) {
		SetOperationResult(false, "Edited ball position is outside the stage bounds.");
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
			SetOperationResult(true, "Updated the selected magnet-ball position.");
			return true;
		}
	}
	SetOperationResult(false, "The selected magnet-ball ID does not exist.");
	return false;
}

bool MagnetStageSystem::Save(const std::string& path)
{
	try {
		if (!IsSafeJsonPath(path) || !ValidateStageData(stageData_)) {
			SetOperationResult(false, "Stage path or stage data is invalid.");
			return false;
		}

		nlohmann::json objects = nlohmann::json::array();
		for (std::size_t index = 0; index < stageData_.ballCount; ++index) {
			const MagnetStageBallPlacement& ball = stageData_.balls[index];
			objects.push_back({
				{ "id", ball.id },
				{ "type", kMagnetBallType },
				{ "position", { ball.position.x, ball.position.y, ball.position.z } },
			});
		}

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
				SetOperationResult(false, "Could not create the stage directory.");
				return false;
			}
		}
		if (!JsonFile::Save(path, root)) {
			SetOperationResult(false, "JSON stage save failed.");
			return false;
		}
		dirty_ = false;
		SetOperationResult(true, "Saved the magnet stage JSON.");
		return true;
	}
	catch (const std::exception&) {
		SetOperationResult(false, "Stage JSON save failed due to an invalid path or value.");
		return false;
	}
}

bool MagnetStageSystem::Load(const std::string& path)
{
	try {
		if (!IsSafeJsonPath(path)) {
			SetOperationResult(false, "Stage path is invalid.");
			return false;
		}

		nlohmann::json root;
		if (!JsonFile::Load(path, root)) {
			SetOperationResult(false, "JSON stage load failed.");
			return false;
		}
		if (!root.is_object() || !root.contains("schema") ||
			!root["schema"].is_string() || root["schema"].get<std::string>() != kSchemaName ||
			!root.contains("schemaVersion") || !root["schemaVersion"].is_number_unsigned() ||
			root["schemaVersion"].get<uint32_t>() != MagnetStageData::kSchemaVersion ||
			!root.contains("name") || !root["name"].is_string() ||
			!root.contains("bounds") || !root["bounds"].is_object() ||
			!root.contains("generator") || !root["generator"].is_object() ||
			!root.contains("objects") || !root["objects"].is_array() ||
			root["objects"].size() > MagnetStageData::kMaximumBallCount) {
			SetOperationResult(false, "Stage JSON schema is invalid or unsupported.");
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
			SetOperationResult(false, "Stage bounds or generator settings are invalid.");
			return false;
		}
		candidate.generation.seed = generator["seed"].get<uint32_t>();
		candidate.generation.ballCount = root["objects"].size();
		candidate.ballCount = root["objects"].size();

		for (std::size_t index = 0; index < candidate.ballCount; ++index) {
			const nlohmann::json& object = root["objects"][index];
			if (!object.is_object() || !object.contains("id") ||
				!object["id"].is_number_unsigned() || !object.contains("type") ||
				!object["type"].is_string() ||
				object["type"].get<std::string>() != kMagnetBallType) {
				SetOperationResult(false, "Stage contains an unsupported object entry.");
				return false;
			}
			Vector3 position{};
			if (!ReadPosition(object, position)) {
				SetOperationResult(false, "Stage contains an invalid object position.");
				return false;
			}
			position.y = kBallPlaneHeight;
			candidate.balls[index] = {
				object["id"].get<uint32_t>(),
				position,
			};
		}

		if (!ValidateStageData(candidate)) {
			SetOperationResult(false, "Loaded stage failed validation.");
			return false;
		}
		stageData_ = std::move(candidate);
		dirty_ = false;
		SetOperationResult(true, "Loaded the magnet stage JSON.");
		return true;
	}
	catch (const std::exception&) {
		SetOperationResult(false, "Stage JSON load failed due to an invalid path or value.");
		return false;
	}
}

bool MagnetStageSystem::RefreshSaveEntries()
{
	const bool refreshed = RefreshSaveEntriesInternal();
	SetOperationResult(
		refreshed,
		refreshed
			? "Refreshed the stage save list."
			: "Could not refresh the stage save list.");
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
				"Save name must use 1-48 ASCII letters, numbers, '_' or '-'.");
			return false;
		}

		std::error_code error;
		const bool alreadyExists = std::filesystem::exists(savePath, error);
		if (error) {
			SetOperationResult(false, "Could not check whether the stage save exists.");
			return false;
		}
		if (alreadyExists && !allowOverwrite) {
			SetOperationResult(false, "That stage save already exists. Confirm overwrite first.");
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
				? "Saved the named magnet stage."
				: "Saved the stage, but the save list could not be refreshed.");
		return true;
	}
	catch (const std::exception&) {
		SetOperationResult(false, "Named stage save failed due to an invalid path or value.");
		return false;
	}
}

bool MagnetStageSystem::LoadNamed(const std::string& saveName)
{
	std::string savePath;
	if (!BuildSavePath(saveName, savePath)) {
		SetOperationResult(false, "Selected stage save name is invalid.");
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

bool MagnetStageSystem::IsFinitePosition(const Vector3& position) noexcept
{
	return std::isfinite(position.x) && std::isfinite(position.y) &&
		std::isfinite(position.z) &&
		std::abs(position.x) <= kMaximumAbsoluteCoordinate &&
		std::abs(position.z) <= kMaximumAbsoluteCoordinate;
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
		stageData.ballCount > stageData.balls.size()) {
		return false;
	}
	MagnetStageGenerationSettings validationSettings = stageData.generation;
	validationSettings.ballCount = (std::max)(stageData.ballCount, std::size_t{ 1 });
	if (!ValidateGenerationSettings(validationSettings)) {
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
