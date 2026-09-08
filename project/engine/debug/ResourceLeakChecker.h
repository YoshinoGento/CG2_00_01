#pragma once

class ResourceLeakChecker {
public:
	ResourceLeakChecker() = default;
	~ResourceLeakChecker();
	void ReportLiveObjects();

	ResourceLeakChecker(const ResourceLeakChecker&) = delete;
	ResourceLeakChecker& operator=(const ResourceLeakChecker&) = delete;

private:
	bool reported_ = false;
};
