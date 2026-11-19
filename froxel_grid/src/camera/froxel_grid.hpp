// Froxel grid of one camera and the world <-> froxel mapping.
// Froxel coordinates are continuous: froxel (i, j, k) covers [i, i+1) x [j, j+1) x [k, k+1).
// Depth slices are exponential: depth(k) = near * (far / near)^(k / nz).
#pragma once

#include "camera/camera.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace ff::camera {

struct FroxelCoord {
    glm::vec3 uvw{0.0f};  // u in [0, nx), v in [0, ny), w in [0, nz) when inside
    bool inside = false;  // finite half-open coordinates: safe to floor to a valid cell index
};

struct FroxelGrid {
    uint32_t nx = 160, ny = 90, nz = 64;

    // Dimensions must be in [1, 2^23], so both integer boundaries and i+0.5 centers are exact floats.
    // Texture/device limits are a separate constraint checked by the GPU allocation code.
    void validate() const;

    // Depth (distance along the view direction) of slice boundary k, k in [0, nz]. Uses the camera's near/far.
    static float sliceDepth(const Camera& cam, uint32_t nz, float k);
    // Inverse of sliceDepth: continuous slice coordinate w of a view depth d.
    static float depthToSlice(const Camera& cam, uint32_t nz, float d);

    float sliceDepth(const Camera& cam, float k) const { return sliceDepth(cam, nz, k); }
    float depthToSlice(const Camera& cam, float d) const { return depthToSlice(cam, nz, d); }

    FroxelCoord worldToFroxel(const Camera& cam, glm::vec3 world) const;
    // Finite coordinates may extrapolate outside the grid. Invalid inputs throw
    // std::invalid_argument; an unrepresentable inverse result throws std::overflow_error.
    glm::vec3 froxelToWorld(const Camera& cam, glm::vec3 uvw) const;
    glm::vec3 froxelCenter(const Camera& cam, uint32_t i, uint32_t j, uint32_t k) const;

    // Largest edge length of a froxel in slice k (world units), measured at the slice's center depth.
    float froxelSize(const Camera& cam, uint32_t k) const;
    // Largest on-screen (horizontal or vertical) edge at the slice's center depth.
    float froxelScreenSize(const Camera& cam, uint32_t k) const;
};

// Number of extra detail layers (noise octaves) turbulence synthesis adds for a froxel:
// max(0, round(log2(worldCell / froxelSize))). Each layer halves the swirl size.
// Finite nonpositive sizes give zero; NaN and infinity throw std::invalid_argument.
int detailLayers(float froxelSize, float worldCell);

// Camera + grid parameters for a WGSL uniform block (16-byte aligned, std140-compatible layout).
struct alignas(16) FroxelUniform {
    float view[16];      // world -> view, column-major
    float invView[16];   // view -> world, column-major
    float tanHalfFovY;
    float aspect;
    float zNear;
    float zFar;
    uint32_t nx, ny, nz;
    uint32_t pad = 0;
};
static_assert(std::is_standard_layout_v<FroxelUniform>);
static_assert(alignof(FroxelUniform) == 16);
static_assert(sizeof(FroxelUniform) == 160);
static_assert(offsetof(FroxelUniform, view) == 0);
static_assert(offsetof(FroxelUniform, invView) == 64);
static_assert(offsetof(FroxelUniform, tanHalfFovY) == 128);
static_assert(offsetof(FroxelUniform, aspect) == 132);
static_assert(offsetof(FroxelUniform, zNear) == 136);
static_assert(offsetof(FroxelUniform, zFar) == 140);
static_assert(offsetof(FroxelUniform, nx) == 144);
static_assert(offsetof(FroxelUniform, ny) == 148);
static_assert(offsetof(FroxelUniform, nz) == 152);
static_assert(offsetof(FroxelUniform, pad) == 156);

FroxelUniform makeUniform(const Camera& cam, const FroxelGrid& grid);

}  // namespace ff::camera
