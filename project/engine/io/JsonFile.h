#pragma once

#include "externals/nlohmann/json.hpp"

#include <string>

namespace JsonFile {
	bool Exists(const std::string& path);
	bool Load(const std::string& path, nlohmann::json& outJson);
	bool Save(const std::string& path, const nlohmann::json& json, int indent = 4);
}
