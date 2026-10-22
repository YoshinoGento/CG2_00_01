#include "farm/core/FarmGrid.h"
#include "3d/Object3d.h"
#include "base/Framework.h"
#include "scene/GamePlayScene.h"
#include "gameplay/GamePlaySessionSystem.h"
#include <cassert>
#include <concepts>
#include <limits>
#include <iostream>
#include <utility>

template<class T> concept MutableTileApi = requires(T& object) { object.GetMutableTile(0); };
template<class T> concept RawMaterialApi = requires(T& object) { object.GetMaterialData(); };
template<class T> concept RawTransformApi = requires(T& object) { object.GetTransformationMatrixData(); };
template<class T> concept PublicBoneFlag = requires(T& object) { object.isBoneManualControl_ = true; };
template<class T> concept PublicRuntimeState = requires(T& object) { object.RuntimeContext(); };
template<class T> concept PublicSceneCamera = requires(T& object) { object.camera_.reset(); };
template<class Base> struct OwnershipProbe : Base {
    static constexpr bool canResetDevice = requires(OwnershipProbe& probe) { probe.dxCommon_.reset(); };
};
static_assert(!MutableTileApi<farm::FarmGrid>);
static_assert(std::same_as<decltype(std::declval<farm::FarmGrid&>().GetTile(0)), const farm::FarmTile*>);
static_assert(std::same_as<decltype(std::declval<farm::FarmGrid&>().GetSelectedTile()), const farm::FarmTile*>);
static_assert(!RawMaterialApi<Object3d> && !RawTransformApi<Object3d> && !PublicBoneFlag<Object3d>);
static_assert(std::same_as<decltype(std::declval<Object3d&>().GetSkeleton()), const std::optional<Skeleton>&>);
static_assert(std::same_as<decltype(std::declval<Object3d&>().GetAnimationTime()), float>);
static_assert(std::same_as<decltype(std::declval<Object3d&>().GetAnimationSpeed()), float>);
static_assert(!OwnershipProbe<Framework>::canResetDevice);
static_assert(!PublicRuntimeState<GamePlaySessionSystem> && !PublicSceneCamera<GamePlayScene>);
static_assert(std::same_as<decltype(std::declval<GamePlayScene&>().GetCamera()), const Camera*>);
static_assert(std::same_as<decltype(std::declval<GamePlaySessionSystem&>().GetCamera()), const Camera*>);

int main() {
    farm::FarmGrid grid;
    assert(grid.Initialize(2, 1));
    farm::FarmTile original;
    original.heightLevel = 1;
    original.state = farm::FarmTileState::Planted;
    original.crop = farm::CropType::Carrot;
    original.moisture = .55f;
    original.growth = .25f;
    assert(grid.SetTile(0, original));
    const auto unchanged = [&] {
        const auto& tile = *grid.GetTile(0);
        assert(tile.heightLevel == original.heightLevel && tile.state == original.state &&
            tile.crop == original.crop && tile.moisture == original.moisture && tile.growth == original.growth);
    };
    for (float value : {-1.f, 2.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        for (int field = 0; field < 4; ++field) {
            auto invalid = original;
            float* fields[] = {&invalid.moisture, &invalid.growth, &invalid.waterAmount, &invalid.soilNutrients};
            *fields[field] = value;
            assert(!grid.SetTile(0, invalid));
            unchanged();
            farm::FarmGrid::Snapshot snapshot;
            grid.CaptureSnapshot(snapshot);
            snapshot.tiles[1] = invalid;
            snapshot.tiles[0].growth = .8f;
            assert(!grid.RestoreSnapshot(snapshot));
            unchanged();
        }
    }
    for (int height : {-999, 3}) {
        auto invalid = original; invalid.heightLevel = height;
        assert(!grid.SetTile(0, invalid)); unchanged();
    }
    auto invalid = original;
    invalid.crop = static_cast<farm::CropType>(99);
    assert(!grid.SetTile(0, invalid)); unchanged();
    invalid = original; invalid.state = static_cast<farm::FarmTileState>(99);
    assert(!grid.SetTile(0, invalid)); unchanged();
    assert(!grid.SetTile(-1, original) && !grid.SetTile(2, original));
    assert(!grid.GetTile(-1) && !grid.GetTile(2));
    auto detached = *grid.GetTile(0);
    detached.growth = .75f;
    unchanged();
    assert(grid.SetTile(0, detached) && grid.GetTile(0)->growth == .75f);
    std::cout << "PASS: immutable read APIs, private ownership, rejected writes and atomic restore\n";
}
