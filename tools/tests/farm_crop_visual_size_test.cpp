#include "farm/system/FarmVisualSystem.h"
#include "farm/system/FarmIrrigationSystem.h"
#include "farm/core/FarmGrid.h"
#include "farm/render/FarmMeshLayout.h"
#include "farm/system/FarmHarvestVisualSystem.h"
#include "3d/LineDrawer.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace {
struct Line { Vector3 a, b; Vector4 color; };
std::vector<Line> lines;
float sphereRadius = 0;
Vector3 sphereCenter{};
bool Near(float a, float b) { return std::fabs(a-b) < 0.0001f; }
}
LineDrawer* LineDrawer::GetInstance() { static LineDrawer instance; return &instance; }
void LineDrawer::DrawLine(const Vector3& a, const Vector3& b, const Vector4& c) { lines.push_back({a,b,c}); }
void LineDrawer::DrawWireCube(const Vector3&, float, const Vector4&) {}
void LineDrawer::DrawWireSphere(const Vector3& p, float r, const Vector4&, uint32_t) { sphereCenter=p; sphereRadius=r; }

int main() {
    {
        farm::FarmGrid field; assert(field.Initialize(3,3));
        farm::FarmVisualSystem visual; visual.Initialize({});
        farm::FarmHarvestVisualSystem effects;
        farm::FarmTile before; before.state=farm::FarmTileState::Planted;
        before.crop=farm::CropType::Carrot; before.growth=1; before.heightLevel=1;
        farm::FarmTile after; after.state=farm::FarmTileState::Tilled; after.heightLevel=1;
        for(int i=0;i<9;++i) assert(field.SetTile(i,after));
        FarmToolActionResult result; result.status=FarmToolActionStatus::Harvested; result.tileIndex=0;
        assert(!effects.Start(result,field,visual)); // Read-only evaluation has no committed payload.
        result.harvestedTile=before;
        assert(effects.Start(result,field,visual) && !effects.Start(result,field,visual));
        const auto initial=effects.GetParts(field,0); assert(initial.count==2);
        effects.Update(field,0); effects.Update(field,-1);
        effects.Update(field,std::numeric_limits<float>::quiet_NaN());
        assert(Near(effects.GetParts(field,0).parts[0].position.y,initial.parts[0].position.y));
        effects.Update(field,effects.kPullSeconds);
        const auto pulled=effects.GetParts(field,0);
        assert(pulled.count==2 && pulled.parts[0].position.y>visual.GetTileVisualData(field,0).center.y);
        assert(Near(pulled.parts[0].scale.y,initial.parts[0].scale.y));
        effects.Update(field,effects.kHoldSeconds+effects.kShrinkSeconds*0.5f);
        const auto shrinking=effects.GetParts(field,0);
        assert(shrinking.count==2 && Near(shrinking.parts[0].scale.y,initial.parts[0].scale.y*0.5f));
        assert(Near(shrinking.parts[1].position.y,shrinking.parts[0].position.y+shrinking.parts[0].scale.y*0.96f));
        effects.Update(field,effects.kDuration); assert(effects.GetActiveCount(field)==0);
        for(int i=0;i<9;++i) {
            result.tileIndex=i; assert(effects.Start(result,field,visual)); effects.Update(field,0.01f);
        }
        assert(effects.GetActiveCount(field)==effects.kCapacity);
        assert(effects.GetParts(field,effects.kCapacity).count==0);
        assert(field.SetTile(8,before)); // Undo/replant invalidates that tile immediately.
        assert(effects.GetActiveCount(field)==effects.kCapacity-1);
        assert(field.Initialize(3,3)); assert(effects.GetActiveCount(field)==0);
        assert(field.SetTile(0,after)); result.tileIndex=0;
        assert(effects.Start(result,field,visual)); effects.Clear(); assert(effects.GetActiveCount(field)==0);
        result.status=FarmToolActionStatus::InventoryFull; assert(!effects.Start(result,field,visual));
        result.status=FarmToolActionStatus::Harvested; result.tileIndex=-1; assert(!effects.Start(result,field,visual));
        std::cout << "PASS: harvest pull/hold/shrink, pause, payload, capacity, OOB and stale tile/generation guards\n";
    }
    farm::FarmGrid grid; assert(grid.Initialize(1,1));
    farm::FarmVisualSystem visual; visual.Initialize({});
    farm::FarmIrrigationSystem irrigation; irrigation.Initialize();
    for (auto crop : {farm::CropType::TestCrop, farm::CropType::Carrot}) {
        farm::FarmTile tile;
        tile.state = farm::FarmTileState::Planted; tile.crop = crop; tile.growth = 1;
        tile.heightLevel = 2;
        tile.careHistory.goodSeconds = 10; tile.careHistory.efficiencySeconds = 10;
        tile.careHistory.nutrientGrowth = 0.95f; tile.careHistory.nutrientSupply = 0.95f;
        assert(grid.SetTile(0,tile));
        const auto good = visual.GetTileVisualData(grid,0);
        const auto mesh = farm::BuildFarmCropMeshParts(grid,0,visual);
        assert(mesh.count == 2 && mesh.parts[1].shape == farm::FarmMeshShape::Leaves);
        assert(mesh.parts[0].shape == (crop == farm::CropType::Carrot ? farm::FarmMeshShape::Carrot : farm::FarmMeshShape::Turnip));
        const float buriedFraction = crop == farm::CropType::Carrot ? 0.84f : 0.50f;
        assert(Near(mesh.parts[0].position.y + mesh.parts[0].scale.y * buriedFraction, good.center.y));
        assert(mesh.parts[0].position.y < good.center.y);
        assert(mesh.parts[0].position.y + mesh.parts[0].scale.y > good.center.y);
        assert(Near(mesh.parts[1].position.y, mesh.parts[0].position.y + mesh.parts[0].scale.y * 0.96f));
        for (std::size_t i=0; i<mesh.count; ++i) {
            const auto& p = mesh.parts[i];
            assert(p.scale.x > 0 && p.scale.y > 0 && p.scale.z > 0);
            assert(p.scale.x <= good.halfExtent*0.90f+0.0001f);
            assert(p.scale.z <= good.halfExtent*0.90f+0.0001f);
            assert(p.surface == farm::FarmMeshSurface::Crop && !p.water);
        }
        assert(good.cropScale > 1 && good.cropScale * 0.32f <= good.halfExtent * 0.90f + 0.0001f);
        assert(FarmCropSizeSystem{}.Evaluate(tile).multiplier == 2); // Rendering cap does not change gameplay.
        lines.clear(); sphereRadius=0;
        visual.Draw(grid,irrigation,{},*LineDrawer::GetInstance(),nullptr,false);
        bool foundStem = false;
        for (const auto& line : lines) {
            assert(std::isfinite(line.a.x) && std::isfinite(line.a.y) && std::isfinite(line.b.y));
            if (Near(line.a.y,good.cropAnchor.y) && Near(line.a.x,good.center.x) &&
                Near(line.b.y,good.cropAnchor.y+0.84f*good.cropScale)) {
                foundStem = true;
            }
        }
        assert(foundStem);
        if (crop == farm::CropType::TestCrop) {
            assert(Near(sphereRadius,0.17f*good.cropScale));
            assert(Near(sphereCenter.y-sphereRadius,good.center.y));
        }
        lines.clear(); sphereRadius = 0;
        visual.Draw(grid,irrigation,{},*LineDrawer::GetInstance(),nullptr,false,false);
        assert(lines.empty() && sphereRadius == 0); // Solid rendering disables the duplicate wire crop.
        assert(grid.GetTile(0)->growth == tile.growth && grid.GetTile(0)->careHistory.nutrientSupply == tile.careHistory.nutrientSupply);
        tile.careHistory.nutrientSupply = 0;
        assert(grid.SetTile(0,tile));
        const auto poor = visual.GetTileVisualData(grid,0);
        assert(farm::BuildFarmCropMeshParts(grid,0,visual).parts[0].scale.x < mesh.parts[0].scale.x);
        assert(poor.cropScale == 0.5f && poor.cropScale < good.cropScale);
        assert(poor.cropAnchor.y == good.cropAnchor.y);
        tile.careHistory = {}; assert(grid.SetTile(0,tile));
        assert(visual.GetTileVisualData(grid,0).cropScale == 1);
        tile.growth = 0.05f;
        tile.careHistory.goodSeconds=1; tile.careHistory.efficiencySeconds=1;
        tile.careHistory.nutrientGrowth=0.01f; tile.careHistory.nutrientSupply=0.01f;
        assert(grid.SetTile(0,tile));
        assert(Near(visual.GetTileVisualData(grid,0).cropScale,1.05f));
        assert(farm::BuildFarmCropMeshParts(grid,0,visual).count == 1);
        grid.GetMutableTile(0)->growth = std::numeric_limits<float>::quiet_NaN();
        assert(std::isfinite(visual.GetTileVisualData(grid,0).cropScale));
        assert(farm::BuildFarmCropMeshParts(grid,0,visual).count == 0);
    }
    assert(!visual.GetTileVisualData(grid,-1).valid && !visual.GetTileVisualData(grid,1).valid);
    assert(farm::BuildFarmCropMeshParts(grid,-1,visual).count == 0);
    assert(farm::BuildFarmCropMeshParts(grid,1,visual).count == 0);
    {
        farm::FarmGrid slopes; assert(slopes.Initialize(3,3));
        for (auto crop : {farm::CropType::Carrot, farm::CropType::TestCrop})
        for (int height : {0,1,2}) for (float growth : {0.05f,0.4f,0.8f,1.f})
        for (float care : {0.f,1.f}) for (float tileSize : {0.1f,1.f,2.f}) {
            farm::FarmVisualLayout layout; layout.tileSize=tileSize;
            farm::FarmVisualSystem scaled; scaled.Initialize(layout);
            farm::FarmTile cropTile; cropTile.state=farm::FarmTileState::Planted; cropTile.crop=crop;
            cropTile.growth=growth; cropTile.heightLevel=height;
            cropTile.careHistory.goodSeconds=10; cropTile.careHistory.efficiencySeconds=10;
            cropTile.careHistory.nutrientGrowth=1; cropTile.careHistory.nutrientSupply=care;
            assert(slopes.SetTile(4,cropTile));
            farm::FarmTile neighbor; neighbor.heightLevel=height==0 ? 2 : 0;
            assert(slopes.SetTile(3,neighbor));
            assert(slopes.GetTile(3)->heightLevel==neighbor.heightLevel);
            const auto surface=farm::BuildFarmSoilSurface(slopes,4,scaled);
            const auto data=scaled.GetTileVisualData(slopes,4);
            const auto parts=farm::BuildFarmCropMeshParts(slopes,4,scaled);
            assert(surface.valid && parts.count>0 && parts.count<=2);
            // Neighbour height changes only the border slopes; planting remains on the central plateau.
            assert(Near(surface.At(1,1).y,data.center.y) && Near(surface.At(2,2).y,data.center.y));
            const auto& leaves=parts.parts[parts.count-1];
            assert(leaves.position.y>=data.center.y);
            if(parts.count==2) {
                const auto& root=parts.parts[0];
                const float buried=crop==farm::CropType::Carrot ? 0.84f : 0.5f;
                assert(Near((data.center.y-root.position.y)/root.scale.y,buried));
                assert(leaves.position.y<=root.position.y+root.scale.y);
                assert(Near(leaves.position.y,root.position.y+root.scale.y*0.96f));
                assert(root.scale.x< data.halfExtent*0.86f); // Root footprint stays away from ramps.
            } else assert(Near(leaves.position.y,data.cropAnchor.y));
            for(std::size_t i=0;i<parts.count;++i) {
                const auto& part=parts.parts[i];
                assert(std::isfinite(part.position.x) && std::isfinite(part.position.y) && std::isfinite(part.position.z));
                assert(part.scale.x>0 && part.scale.y>0 && part.scale.z>0);
            }
            assert(slopes.GetTile(4)->growth==growth && slopes.GetTile(4)->heightLevel==height);
        }
    }
    farm::FarmTile empty; assert(grid.SetTile(0,empty));
    assert(farm::BuildFarmCropMeshParts(grid,0,visual).count == 0);
    empty.feature = farm::FarmTileFeature::Canal; assert(grid.SetTile(0,empty));
    assert(farm::BuildFarmCropMeshParts(grid,0,visual).count == 0);
    std::cout << "PASS: planted root burial, leaf attachment, slope plateau, growth/size bounds, legacy wire and invalid inputs\n";
}
