#include "froxel_link/froxel_link.hpp"

#include <algorithm>
#include <cmath>

namespace ff::froxel_link {

float sliceFroxelSize(const Camera& cam, const FroxelGrid& grid, uint32_t k, SizeMeasure measure) {
    return measure == SizeMeasure::OnScreen ? grid.froxelScreenSize(cam, k) : grid.froxelSize(cam, k);
}

int detailLayers(float froxelSize, float worldCell) { return camera::detailLayers(froxelSize, worldCell); }

float sliceDepth(const Camera& cam, const FroxelGrid& grid, float k) { return grid.sliceDepth(cam, k); }

glm::vec3 froxelCenter(const Camera& cam, const FroxelGrid& grid, uint32_t i, uint32_t j, uint32_t k) {
    return grid.froxelCenter(cam, i, j, k);
}

std::optional<glm::vec3> locate(const Camera& cam, const FroxelGrid& grid, glm::vec3 world) {
    const camera::FroxelCoord c = grid.worldToFroxel(cam, world);
    if (!c.inside) return std::nullopt;
    return c.uvw;
}

std::optional<uint32_t> sliceAt(const Camera& cam, const FroxelGrid& grid, glm::vec3 world) {
    const std::optional<glm::vec3> uvw = locate(cam, grid, world);
    if (!uvw) return std::nullopt;
    // Half-open bounds: floor is a valid index; the min only guards against rounding.
    return std::min(grid.nz - 1, uint32_t(std::floor(uvw->z)));
}

}  // namespace ff::froxel_link
