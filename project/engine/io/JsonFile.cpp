#include "JsonFile.h"

#include "base/Logger.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

namespace {
	std::filesystem::path Utf8Path(const std::string& value)
	{
		return std::filesystem::path(std::u8string(
			reinterpret_cast<const char8_t*>(value.data()), value.size()));
	}
}

namespace JsonFile {
	bool Exists(const std::string& path)
	{
		std::error_code error;
		return std::filesystem::exists(Utf8Path(path), error) && !error;
	}

	bool Load(const std::string& path, nlohmann::json& outJson)
	{
		if (!Exists(path)) {
			Logger::Log("JsonFile::Load failed. File does not exist: " + path);
			return false;
		}

		std::ifstream file(Utf8Path(path));
		if (!file.is_open()) {
			Logger::Log("JsonFile::Load failed. Could not open file: " + path);
			return false;
		}

		try {
			nlohmann::json loadedJson;
			file >> loadedJson;
			outJson = std::move(loadedJson);
		}
		catch (const std::exception& e) {
			Logger::Log("JsonFile::Load failed. Parse error in " + path + ": " + e.what());
			return false;
		}

		return true;
	}

	bool Save(const std::string& path, const nlohmann::json& json, int indent)
	{
		std::ofstream file(Utf8Path(path));
		if (!file.is_open()) {
			Logger::Log("JsonFile::Save failed. Could not open file: " + path);
			return false;
		}

		try {
			file << json.dump(indent);
		}
		catch (const std::exception& e) {
			Logger::Log("JsonFile::Save failed. Write error in " + path + ": " + e.what());
			return false;
		}

		if (!file.good()) {
			Logger::Log("JsonFile::Save failed. Stream error after writing: " + path);
			return false;
		}

		return true;
	}
}
