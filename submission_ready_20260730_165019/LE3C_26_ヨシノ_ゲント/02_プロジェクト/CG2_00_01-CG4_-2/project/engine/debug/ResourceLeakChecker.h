#pragma once

class ResourceLeakChecker {
public:
	ResourceLeakChecker() = default;
	~ResourceLeakChecker();

	ResourceLeakChecker(const ResourceLeakChecker&) = delete;
	ResourceLeakChecker& operator=(const ResourceLeakChecker&) = delete;
};
