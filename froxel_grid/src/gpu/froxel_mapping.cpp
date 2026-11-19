#include "gpu/froxel_mapping.hpp"
#include "camera/froxel_grid.hpp"

#include <ff_froxel_shaders.hpp>

#include <cstddef>

namespace ff::gpu {
namespace {
static_assert(sizeof(camera::FroxelUniform) == 160);
static_assert(offsetof(camera::FroxelUniform, invView) == 64);
static_assert(offsetof(camera::FroxelUniform, tanHalfFovY) == 128);
static_assert(offsetof(camera::FroxelUniform, nx) == 144);
}  // namespace

std::string withFroxelMapping(std::string_view kernel) {
    std::string result(ff::froxel_shaders::froxel_mapping_wgsl);
    result += '\n';
    result.append(kernel);
    return result;
}

}  // namespace ff::gpu
