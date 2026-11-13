// World -> camera density only; no solver, velocity transport, or history.
#pragma once

#include "camera/froxel_grid.hpp"
#include "gpu/float_volume.hpp"
#include "gpu/timer.hpp"

namespace ff::gpu {

struct WorldGridGeometry {
    glm::vec3 origin{0.0f};
    float cellSize = 1.0f;
    void validate(Extent3D extent) const;
};

// The caller supplies cell-centered scalar density as a GPU texture.
// MAC velocities require distinct face textures/offsets and are not this input.
class FroxelDensitySampler {
public:
    explicit FroxelDensitySampler(const GpuContext& ctx);

    // Cell-centered trilinear density; clamp at boundary cells inside the closed world box,
    // zero outside. Separate output must match the froxel grid and must not alias world.
    // Input/output resources belong to ctx. One compute pass, no readback on this path.
    void dispatch(const GpuContext& ctx, const camera::Camera& camera, const camera::FroxelGrid& grid,
                  const FloatVolume& world, const WorldGridGeometry& geometry, FloatVolume& output,
                  GpuTimer* timer = nullptr);

private:
    WGPUDevice owner_ = nullptr;
    BindGroupLayout layout_;
    PipelineLayout pipelineLayout_;
    ComputePipeline pipeline_;
    Buffer cameraUniform_, worldUniform_;
};

}  // namespace ff::gpu
