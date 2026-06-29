#include "farm/render/FarmRenderer.h"

#include "farm/core/FarmGrid.h"

namespace farm {

void FarmRenderer::Draw(const FarmGrid& grid) {
	lastDrawTileCount_ = visible_ ? grid.GetTileCount() : 0;
}

} // namespace farm
