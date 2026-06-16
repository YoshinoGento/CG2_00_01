#include "3d/SkyboxManager.h"

#include "base/ImGuiManager.h"

#include <algorithm>
#include <cassert>
#include <iterator>

void SkyboxManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    assert(dxCommon);
    assert(srvManager);

    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    skyboxes_.clear();
    currentSkyboxIndex_ = 0;
}

void SkyboxManager::LoadSkybox(const std::string& name, const std::string& texturePath) {
    assert(dxCommon_);
    assert(srvManager_);

    SkyboxData data{};
    data.name = name;
    data.texturePath = texturePath;
    data.skybox = std::make_unique<Skybox>();
    data.skybox->Initialize(dxCommon_, srvManager_, texturePath);
    skyboxes_.push_back(std::move(data));
}

void SkyboxManager::SetCurrentSkybox(uint32_t index) {
    if (skyboxes_.empty()) {
        currentSkyboxIndex_ = 0;
        return;
    }
    currentSkyboxIndex_ = (std::min)(index, static_cast<uint32_t>(skyboxes_.size() - 1));
}

void SkyboxManager::SetCurrentSkybox(const std::string& name) {
    const auto it = std::find_if(
        skyboxes_.begin(),
        skyboxes_.end(),
        [&](const SkyboxData& data) { return data.name == name; });
    if (it == skyboxes_.end()) {
        return;
    }
    currentSkyboxIndex_ = static_cast<uint32_t>(std::distance(skyboxes_.begin(), it));
}

void SkyboxManager::Update(Camera* camera) {
    if (!camera || skyboxes_.empty()) {
        return;
    }
    SetCurrentSkybox(currentSkyboxIndex_);
    skyboxes_[currentSkyboxIndex_].skybox->Update(camera);
}

void SkyboxManager::Draw() {
    if (skyboxes_.empty()) {
        return;
    }
    SetCurrentSkybox(currentSkyboxIndex_);
    skyboxes_[currentSkyboxIndex_].skybox->Draw();
}

void SkyboxManager::DrawImGui() {
    if (skyboxes_.empty()) {
        ImGui::TextUnformatted("Skybox: none");
        return;
    }

    SetCurrentSkybox(currentSkyboxIndex_);
    ImGui::Text("Current: %s", skyboxes_[currentSkyboxIndex_].name.c_str());
    ImGui::Text("Texture: %s", skyboxes_[currentSkyboxIndex_].texturePath.c_str());

    std::vector<const char*> names;
    names.reserve(skyboxes_.size());
    for (const SkyboxData& skybox : skyboxes_) {
        names.push_back(skybox.name.c_str());
    }

    int current = static_cast<int>(currentSkyboxIndex_);
    if (ImGui::Combo("Skybox Preset", &current, names.data(), static_cast<int>(names.size()))) {
        SetCurrentSkybox(static_cast<uint32_t>((std::max)(current, 0)));
    }
}

const std::string& SkyboxManager::GetCurrentSkyboxName() const {
    static const std::string emptyName = "None";
    if (skyboxes_.empty() || currentSkyboxIndex_ >= skyboxes_.size()) {
        return emptyName;
    }
    return skyboxes_[currentSkyboxIndex_].name;
}

uint32_t SkyboxManager::GetCurrentSrvIndex() const {
    if (skyboxes_.empty() || currentSkyboxIndex_ >= skyboxes_.size()) {
        return 0;
    }
    return skyboxes_[currentSkyboxIndex_].skybox->GetSrvIndex();
}
