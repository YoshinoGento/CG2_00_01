#pragma once

#include "3d/Skybox.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Camera;
class DirectXCommon;
class SrvManager;

class SkyboxManager {
public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void LoadSkybox(const std::string& name, const std::string& texturePath);
    void SetCurrentSkybox(uint32_t index);
    void SetCurrentSkybox(const std::string& name);
    void Update(Camera* camera);
    void Draw();
    void DrawImGui();

    uint32_t GetCurrentSkyboxIndex() const { return currentSkyboxIndex_; }
    const std::string& GetCurrentSkyboxName() const;
    uint32_t GetCurrentSrvIndex() const;

private:
    struct SkyboxData {
        std::string name;
        std::string texturePath;
        std::unique_ptr<Skybox> skybox;
    };

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    std::vector<SkyboxData> skyboxes_;
    uint32_t currentSkyboxIndex_ = 0;
};
