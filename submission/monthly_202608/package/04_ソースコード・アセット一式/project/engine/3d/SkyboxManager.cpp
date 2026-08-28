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
    mode_ = SkyboxMode::SolidColor;
}

void SkyboxManager::LoadSkybox(const std::string& name, const std::string& texturePath) {
    assert(dxCommon_);
    assert(srvManager_);

    SkyboxData data{};
    data.name = name;
    data.texturePath = texturePath;
    data.skybox = std::make_unique<Skybox>();
    data.skybox->Initialize(dxCommon_, texturePath);
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
    if (name == "Sunny") {
        mode_ = SkyboxMode::Sunny;
    } else if (name == "Evening") {
        mode_ = SkyboxMode::Evening;
    } else if (name == "Night") {
        mode_ = SkyboxMode::Night;
    } else if (name == "Storm") {
        mode_ = SkyboxMode::Storm;
    }
}

void SkyboxManager::SetMode(SkyboxMode mode) {
    mode_ = mode;
    switch (mode_) {
    case SkyboxMode::Sunny:
        SetCurrentSkybox("Sunny");
        break;
    case SkyboxMode::Evening:
        SetCurrentSkybox("Evening");
        break;
    case SkyboxMode::Night:
        SetCurrentSkybox("Night");
        break;
    case SkyboxMode::Storm:
        SetCurrentSkybox("Storm");
        break;
    case SkyboxMode::None:
    case SkyboxMode::SolidColor:
    default:
        break;
    }
}

void SkyboxManager::CycleMode() {
    const int modeCount = static_cast<int>(SkyboxMode::Storm) + 1;
    int next = static_cast<int>(mode_) + 1;
    if (next >= modeCount) {
        next = 0;
    }
    SetMode(static_cast<SkyboxMode>(next));
}

void SkyboxManager::Update(Camera* camera) {
    if (!camera || skyboxes_.empty() || mode_ == SkyboxMode::None || mode_ == SkyboxMode::SolidColor) {
        return;
    }
    SetCurrentSkybox(currentSkyboxIndex_);
    skyboxes_[currentSkyboxIndex_].skybox->Update(camera);
}

void SkyboxManager::Draw() {
    if (skyboxes_.empty() || mode_ == SkyboxMode::None || mode_ == SkyboxMode::SolidColor) {
        return;
    }
    SetCurrentSkybox(currentSkyboxIndex_);
    skyboxes_[currentSkyboxIndex_].skybox->Draw();
}

void SkyboxManager::DrawImGui() {
#ifdef USE_IMGUI
    static const char* modeNames[] = { "None", "SolidColor", "Sunny", "Evening", "Night", "Storm" };
    int mode = static_cast<int>(mode_);
    if (ImGui::Combo("Skybox Mode", &mode, modeNames, _countof(modeNames))) {
        mode = std::clamp(mode, 0, static_cast<int>(_countof(modeNames)) - 1);
        SetMode(static_cast<SkyboxMode>(mode));
    }

    ImGui::Text("Current Mode: %s", GetModeName());
    ImGui::Text("Clear Color: %.2f, %.2f, %.2f",
        GetClearColor().x,
        GetClearColor().y,
        GetClearColor().z);
    ImGui::TextUnformatted("F6: Cycle Skybox Mode");

    if (skyboxes_.empty()) {
        ImGui::TextUnformatted("Skybox texture list: none");
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
#endif
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
    const TextureCubeHandle textureHandle = skyboxes_[currentSkyboxIndex_].skybox->GetTextureHandle();
    return textureHandle.IsValid() ? textureHandle.Index() : 0;
}

const char* SkyboxManager::GetModeName() const {
    switch (mode_) {
    case SkyboxMode::None:
        return "None";
    case SkyboxMode::SolidColor:
        return "SolidColor";
    case SkyboxMode::Sunny:
        return "Sunny";
    case SkyboxMode::Evening:
        return "Evening";
    case SkyboxMode::Night:
        return "Night";
    case SkyboxMode::Storm:
        return "Storm";
    default:
        return "Unknown";
    }
}

Vector4 SkyboxManager::GetClearColor() const {
    switch (mode_) {
    case SkyboxMode::None:
        return { 0.08f, 0.11f, 0.12f, 1.0f };
    case SkyboxMode::SolidColor:
        return { 0.55f, 0.75f, 0.95f, 1.0f };
    case SkyboxMode::Evening:
        return { 0.68f, 0.48f, 0.34f, 1.0f };
    case SkyboxMode::Night:
        return { 0.04f, 0.06f, 0.12f, 1.0f };
    case SkyboxMode::Storm:
        return { 0.25f, 0.30f, 0.36f, 1.0f };
    case SkyboxMode::Sunny:
    default:
        return { 0.52f, 0.72f, 0.95f, 1.0f };
    }
}
