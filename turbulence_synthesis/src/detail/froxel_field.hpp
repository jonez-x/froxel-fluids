// Per-camera froxel detail buffer on the CPU (spec 004, contracts/froxel-detail-buffer.md).
// Same layout as the GPU buffer: vec4 per froxel (detail velocity xyz, layer count w),
// index i + nx * (j + ny * k). Filled by GPU read-back or by the CPU reference.
#pragma once

#include "froxel_link/froxel_link.hpp"
#include "detail/turbulence.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace ff::detail {

class FroxelDetailField {
public:
    FroxelDetailField(const camera::Camera& cam, const camera::FroxelGrid& grid);
    FroxelDetailField(const camera::Camera& cam, const camera::FroxelGrid& grid, std::vector<glm::vec4> data);

    const camera::Camera& camera() const { return cam_; }
    const camera::FroxelGrid& grid() const { return grid_; }
    const std::vector<glm::vec4>& data() const { return data_; }
    std::vector<glm::vec4>& data() { return data_; }

    uint64_t index(uint32_t i, uint32_t j, uint32_t k) const { return i + uint64_t(grid_.nx) * (j + uint64_t(grid_.ny) * k); }
    const glm::vec4& at(uint32_t i, uint32_t j, uint32_t k) const { return data_[index(i, j, k)]; }

    // Trilinear interpolation of the stored detail velocity at world point x (froxel centers at +0.5,
    // clamped at the grid border). Zero outside the frustum.
    glm::dvec3 sample(glm::dvec3 x) const;

private:
    camera::Camera cam_;
    camera::FroxelGrid grid_;
    std::vector<glm::vec4> data_;
};

// CPU reference fill: detail at every froxel center, with the layer count of its slice (multi-threaded).
FroxelDetailField synthesizeFroxelsCpu(const TurbulenceSynthesis& synth, const camera::Camera& cam,
                                       const camera::FroxelGrid& grid);

}  // namespace ff::detail
