#include "froxel_link/gpu_froxel_link.hpp"

#include "gpu/froxel_mapping.hpp"

namespace ff::froxel_link {

FroxelUniform froxelUniform(const Camera& cam, const FroxelGrid& grid) { return camera::makeUniform(cam, grid); }

std::string withFroxelGeometry(std::string_view kernel) { return gpu::withFroxelMapping(kernel); }

}  // namespace ff::froxel_link
