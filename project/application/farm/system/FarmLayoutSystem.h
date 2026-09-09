#pragma once
#include "farm/core/FarmGrid.h"
#include <string>
#include <vector>

struct FarmLayoutEntry {
    std::string id;
    std::string name;
};

// Layout-only persistence. Full game documents are deliberately not accepted here.
class FarmLayoutSystem final {
public:
    bool Initialize(const std::string& directory);
    bool SaveNew(const std::string& name, const farm::FarmGrid& grid);
    bool Load(const std::string& id, farm::FarmGrid& grid);
    const std::vector<FarmLayoutEntry>& Entries() const noexcept { return entries_; }
    const std::string& Error() const noexcept { return error_; }
private:
    bool Refresh();
    std::string directory_;
    std::vector<FarmLayoutEntry> entries_;
    std::string error_;
};
