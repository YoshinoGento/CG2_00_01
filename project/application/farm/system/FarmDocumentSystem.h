#pragma once

#include <string>
#include <vector>

namespace farm {
class FarmGrid;
}

enum class FarmDocumentStatus {
	Ready,
	Modified,
	Saved,
	Loaded,
	Reset,
	Deleted,
	Error,
};

struct FarmDocumentEntry {
	std::string id;
	std::string displayName;
	std::string savedAt;
};

// Owns Farm document persistence and validation. UI code only requests operations.
class FarmDocumentSystem final {
public:
	bool Initialize(const std::string& directoryPath, farm::FarmGrid& grid);
	bool Save(const farm::FarmGrid& grid);
	bool SaveAs(const std::string& displayName, const farm::FarmGrid& grid);
	bool Load(const std::string& documentId, farm::FarmGrid& grid);
	bool Rename(const std::string& documentId, const std::string& displayName);
	bool Delete(const std::string& documentId);
	bool Reset(farm::FarmGrid& grid);
	void MarkDirty() noexcept;

	[[nodiscard]] bool IsDirty() const noexcept { return dirty_; }
	[[nodiscard]] bool FileExists() const noexcept { return fileExists_; }
	[[nodiscard]] bool HasDocuments() const noexcept { return !documents_.empty(); }
	[[nodiscard]] bool HasError() const noexcept { return status_ == FarmDocumentStatus::Error; }
	[[nodiscard]] FarmDocumentStatus GetStatus() const noexcept { return status_; }
	[[nodiscard]] const std::string& GetPath() const noexcept { return path_; }
	[[nodiscard]] const std::string& GetActiveDocumentId() const noexcept { return activeDocumentId_; }
	[[nodiscard]] const std::string& GetDisplayName() const noexcept { return displayName_; }
	[[nodiscard]] const std::string& GetStatusMessage() const noexcept { return message_; }
	[[nodiscard]] const std::vector<FarmDocumentEntry>& GetDocuments() const noexcept { return documents_; }

private:
	bool RefreshDocumentList();
	bool WriteCatalog();
	bool LoadCatalog(std::string& activeDocumentId) const;
	bool SaveToDocument(
		const std::string& documentId,
		const std::string& displayName,
		const farm::FarmGrid& grid);
	void SetStatus(FarmDocumentStatus status, std::string message);
	void SetError(std::string message);

	std::string directoryPath_;
	std::string saveDirectoryPath_;
	std::string catalogPath_;
	std::string path_;
	std::string activeDocumentId_;
	std::string displayName_ = "Untitled Farm";
	std::string message_ = "Ready";
	FarmDocumentStatus status_ = FarmDocumentStatus::Ready;
	std::vector<FarmDocumentEntry> documents_;
	bool dirty_ = false;
	bool fileExists_ = false;
};
