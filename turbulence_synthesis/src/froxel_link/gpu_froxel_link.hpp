// GPU side of the interface between the froxel grid (W2, ../froxel_grid) and the turbulence synthesis
// (spec 004). A detail kernel gets the camera/froxel geometry as one W2 uniform and the W2 WGSL
// helpers, so CPU (froxel_link.hpp) and GPU use the same mapping code instead of copies.
//
// In a kernel built with withFroxelGeometry:
//   struct FroxelUniform                         (160 bytes, layout fixed by W2)
//   fn froxel_center(params, cell: vec3<u32>) -> vec3<f32>      world position of a froxel center
//   fn froxel_slice_depth(params, k: f32) -> f32                view depth of slice coordinate k
//   fn world_to_froxel(params, world) -> FroxelCoord            { uvw, inside }
//   fn froxel_to_world(params, uvw) -> vec3<f32>
// See froxel_grid/shaders/froxel_mapping.wgsl.
#pragma once

#include "froxel_link/froxel_link.hpp"

#include <string>
#include <string_view>

namespace ff::froxel_link {

using camera::FroxelUniform;

// The W2 uniform for one camera and its froxel grid (validates both, throws std::invalid_argument).
FroxelUniform froxelUniform(const Camera& cam, const FroxelGrid& grid);

// Kernel source with the W2 mapping helpers in front of it.
std::string withFroxelGeometry(std::string_view kernel);

}  // namespace ff::froxel_link
