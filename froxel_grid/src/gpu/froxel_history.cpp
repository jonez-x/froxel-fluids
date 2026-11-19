#include "gpu/froxel_history.hpp"
#include "gpu/froxel_mapping.hpp"
#include "camera/history.hpp"

#include <ff_froxel_shaders.hpp>

#include <cstddef>

namespace ff::gpu {
namespace {
static_assert(sizeof(camera::HistoryUniform) == 336 && alignof(camera::HistoryUniform) == 16);
static_assert(offsetof(camera::HistoryUniform, current) == 0);
static_assert(offsetof(camera::HistoryUniform, previous) == 160);
static_assert(offsetof(camera::HistoryUniform, hasHistory) == 320);
static_assert(offsetof(camera::HistoryUniform, identityMapping) == 324);
}  // namespace

std::string withFroxelHistory(std::string_view kernel) {
    std::string suffix(ff::froxel_shaders::froxel_history_wgsl);
    suffix += '\n';
    suffix.append(kernel);
    return withFroxelMapping(suffix);
}

}  // namespace ff::gpu
