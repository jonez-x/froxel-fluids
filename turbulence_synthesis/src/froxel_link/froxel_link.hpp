// Interface between the froxel grid (W2, module ../froxel_grid, read-only) and the turbulence synthesis
// (W5, specs 003/004/009). The detail code reaches the froxel grid only through this header (CPU) and
// froxel_link/gpu_froxel_link.hpp (GPU). If the W2 module changes, only froxel_link/ has to follow.
//
// Data types: camera::Camera and camera::FroxelGrid are the W2 parameter structs (workstreams.md:
// "W2 -> W3, W5, W6: one C++ struct + WGSL uniform, W2 owns it"); they are used as they are.
// Conventions (froxel_grid/README.md): right-handed, +Y up, view along -Z, exponential slices,
// cell centers at i + 0.5, half-open index bounds [0, n), layout i + nx * (j + ny * k).
#pragma once

#include "camera/camera.hpp"
#include "camera/froxel_grid.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <optional>

namespace ff::froxel_link {

using camera::Camera;
using camera::FroxelGrid;

// Which froxel size decides the number of detail layers of a slice.
enum class SizeMeasure {
    LargestEdge,  // largest edge, depth included (FroxelGrid::froxelSize)
    OnScreen,     // largest on-screen edge (FroxelGrid::froxelScreenSize, ideas I27)
};

// Froxel size of slice k (k < grid.nz) at the slice's center depth.
float sliceFroxelSize(const Camera& cam, const FroxelGrid& grid, uint32_t k, SizeMeasure measure);

// Number of octaves between a world cell and a froxel of this size; 0 if the froxel is not smaller.
int detailLayers(float froxelSize, float worldCell);

// View depth of the continuous slice coordinate k in [0, nz] (k + 0.5: center of slice k).
float sliceDepth(const Camera& cam, const FroxelGrid& grid, float k);

// World position of the center of froxel (i, j, k).
glm::vec3 froxelCenter(const Camera& cam, const FroxelGrid& grid, uint32_t i, uint32_t j, uint32_t k);

// Continuous froxel coordinate of a world point (cell centers at i + 0.5); empty outside the grid.
std::optional<glm::vec3> locate(const Camera& cam, const FroxelGrid& grid, glm::vec3 world);

// Index of the slice that contains a world point; empty outside the grid.
std::optional<uint32_t> sliceAt(const Camera& cam, const FroxelGrid& grid, glm::vec3 world);

}  // namespace ff::froxel_link
