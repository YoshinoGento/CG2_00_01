#include "title/TitleFarmRenderer.h"
#include "title/TitleCelestialGeometry.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "base/Logger.h"
#include <filesystem>
#include <string>

namespace title {
TitleFarmRenderer::TitleFarmRenderer() = default;
TitleFarmRenderer::~TitleFarmRenderer() { Finalize(); }

bool TitleFarmRenderer::Initialize(Object3dCommon* common, ModelManager* modelManager,
    TextureManager* textures, const farm::FarmGrid& grid) {
    if (common_ || !common || !modelManager || !textures || !common->GetLightingSystem()) return false;
    constexpr std::array<const char*,11> paths{"farm/unit_box.obj", "farm/triangle_lower.obj",
        "farm/triangle_upper.obj", "farm/crop_turnip.obj", "farm/crop_carrot.obj",
        "farm/crop_turnip_leaves.obj", "farm/crop_carrot_leaves.obj", "farm/crop_tomato.obj",
        "farm/crop_tomato_stems.obj", "farm/crop_pumpkin.obj", "farm/crop_pumpkin_vines.obj"};
    std::error_code error;
    for (std::size_t i=0; i<paths.size(); ++i) {
        // Turnip is not displayed by the staged farm; avoid loading an excluded submission asset.
        if (i == 3 || i == 5) continue;
        if (!std::filesystem::is_regular_file(std::filesystem::path("Resources")/paths[i],error)) return false;
        modelManager->LoadModel(paths[i]); models_[i] = modelManager->GetModel(paths[i]);
        if (!models_[i] || models_[i]->HasSkinCluster()) return false;
    }
    const auto white = textures->LoadTexture2D("Resources/farm/white.png");
    if (!white.IsValid() || white == textures->GetFallback2D()) return false;
    if (!std::filesystem::is_regular_file("Resources/title/countryside.obj",error) ||
        !std::filesystem::is_regular_file("Resources/title/countryside_atlas.png",error)) return false;
    modelManager->LoadModel("title/countryside.obj");
    auto* landscapeModel = modelManager->GetModel("title/countryside.obj");
    const auto landscapeTexture = textures->LoadTexture2D("Resources/title/countryside_atlas.png");
    if (!landscapeModel || landscapeModel->HasSkinCluster() ||
        !landscapeTexture.IsValid() || landscapeTexture == textures->GetFallback2D()) return false;
    landscape_ = std::make_unique<Object3d>();
    landscape_->Initialize(common);
    landscape_->SetModel(landscapeModel);
    landscape_->SetTexture(landscapeTexture);
    landscape_->SetSpecularType(Object3d::SpecularType::None);
    landscape_->SetEnableLighting(true);
    if (!sky_.Initialize(common->GetDxCommon(),textures)) return false;
    common_ = common; lights_ = common->GetLightingSystem();
    previousDirectional_ = lights_->GetDirectionalLight(); previousSpot_ = lights_->GetSpotLight();
    previousShadowStrength_ = common_->GetShadowStrength();
    camera_.SetTranslate({6.5f,3.5f,-6.5f}); camera_.SetRotate({.10f,-.45f,0});
    camera_.SetAspectRatio(kWidth/kHeight); camera_.SetFovY(.72f);
    camera_.SetNearClip(.1f); camera_.SetFarClip(180); camera_.Update();
    farm::FarmVisualLayout layout;
    layout.center = {0,0,6}; layout.tileGap = .05f; layout.heightStep = .27f;
    farm::FarmVisualSystem visual; visual.Initialize(layout);
    parts_.reserve(kMaximumParts);
    for (int index=0; index<grid.GetTileCount(); ++index) {
        const auto terrain = farm::BuildFarmTileMeshParts(grid,index,visual);
        const auto crops = farm::BuildFarmCropMeshParts(grid,index,visual);
        if (parts_.size()+terrain.count+crops.count+4 > kMaximumParts) return false;
        parts_.insert(parts_.end(),terrain.parts.begin(),terrain.parts.begin()+terrain.count);
        parts_.insert(parts_.end(),crops.parts.begin(),crops.parts.begin()+crops.count);
    }
    for (std::size_t row=0; row<waterCenters_.size(); ++row)
        waterCenters_[row] = visual.GetTileVisualData(grid,static_cast<int>(row)*5+2).waterSurfaceCenter;
    const auto source = waterCenters_[0];
    const Vector4 wood{.37f,.25f,.12f,1};
    for (int side : {-1,1}) parts_.push_back({{source.x+side*.40f,source.y+.26f,source.z},
        {.045f,.28f,.045f},wood});
    parts_.push_back({{source.x,source.y+.48f,source.z},{.50f,.045f,.07f},wood});
    objects_.reserve(parts_.size());
    for (const auto& part : parts_) {
        const auto shape = static_cast<std::size_t>(part.shape);
        if (shape >= models_.size() || !models_[shape]) return false;
        auto object = std::make_unique<Object3d>(); object->Initialize(common_);
        object->SetModel(models_[shape]); object->SetTexture(white);
        object->SetPosition(part.position);
        if (!object->SetScale(part.scale) || !object->SetShearY(part.slope)) return false;
        object->SetSpecularType(Object3d::SpecularType::None);
        object->SetEnableLighting(!part.water);
        objects_.push_back(std::move(object));
    }
    for (auto& glint : glints_) {
        glint = std::make_unique<Object3d>(); glint->Initialize(common_);
        glint->SetModel(models_[0]); glint->SetTexture(white);
        if (!glint->SetScale({.10f,.002f,.012f})) return false;
        glint->SetEnableLighting(false); glint->SetSpecularType(Object3d::SpecularType::None);
    }
    Logger::Log("TitleFarmRenderer: retained objects="+std::to_string(objects_.size())+", glints=8.\n");
    ready_ = true;
    return true;
}

void TitleFarmRenderer::Finalize() noexcept {
    if (lights_) { lights_->SetDirectionalLight(previousDirectional_); lights_->SetSpotLight(previousSpot_); }
    if (common_) common_->SetShadowStrength(previousShadowStrength_);
    lights_ = nullptr; common_ = nullptr; ready_ = false;
    objects_.clear(); parts_.clear();
    landscape_.reset();
    for (auto& glint : glints_) glint.reset();
}

void TitleFarmRenderer::Update(const Frame& frame) {
    if (!ready_) return;
    const auto& projection = camera_.GetProjectionMatrix();
    const auto& cameraWorld = camera_.GetWorldMatrix();
    const auto lighting = MakeCelestialLighting(CelestialDirection(frame.sun, projection, cameraWorld),
        CelestialDirection(frame.moon, projection, cameraWorld),frame.groundExposure);
    lights_->SetDirectionalLight({frame.lightColor,lighting.direction,lighting.intensity});
    common_->SetShadowStrength(lighting.shadowStrength);
    auto spot = previousSpot_; spot.intensity = 0; lights_->SetSpotLight(spot);
    lights_->SetCameraPosition(camera_.GetTranslate());
    sky_.Update(frame,camera_);
    landscape_->Update(&camera_,0);
    for (std::size_t i=0; i<parts_.size(); ++i) {
        const auto& part = parts_[i]; auto& object = *objects_[i];
        object.SetColor(part.water ? frame.waterColor : part.color);
        const bool foliage = part.shape == farm::FarmMeshShape::CarrotLeaves ||
            part.shape == farm::FarmMeshShape::TomatoStems || part.shape == farm::FarmMeshShape::PumpkinVines;
        object.SetRotation({0,0,foliage ? frame.windAngle : 0});
        object.Update(&camera_,0);
    }
    for (std::size_t i=0; i<glints_.size(); ++i) {
        auto position = waterCenters_[i/2];
        const float phase = std::fmod(frame.waterPhase + static_cast<float>(i%2)*.5f,1.0f);
        position.x += i%2 ? .12f : -.17f;
        position.z += (phase-.5f)*.64f; position.y += .014f;
        glints_[i]->SetPosition(position);
        const float brightness = .45f+.38f*std::sin(phase*3.14159265f);
        glints_[i]->SetColor({brightness*(1-frame.nightAmount*.4f),brightness,brightness,1});
        glints_[i]->Update(&camera_,0);
    }
}

void TitleFarmRenderer::Draw() {
    if (!ready_) return;
    common_->UpdateDirectionalShadow(lights_->GetDirectionalLight().direction,{0,0,6});
    if (common_->BeginShadowPass()) {
        landscape_->DrawShadow();
        for (std::size_t i=0; i<parts_.size(); ++i) if (!parts_[i].water) objects_[i]->DrawShadow();
        common_->EndShadowPass();
    }
    sky_.Draw();
    common_->BeginObjectPass();
    landscape_->Draw();
    for (const auto& object : objects_) object->Draw();
    for (const auto& glint : glints_) glint->Draw();
    common_->EndObjectPass();
}
}
