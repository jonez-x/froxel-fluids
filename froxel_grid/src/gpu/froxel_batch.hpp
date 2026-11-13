// Native W2 integration: immutable per-camera parameters and caller-owned encoding.
#pragma once

#include "gpu/froxel_density.hpp"

#include <span>

namespace ff::gpu {

struct FroxelDensityJob {
    camera::Camera camera;
    camera::FroxelGrid grid;
    const FloatVolume* world = nullptr;
    WorldGridGeometry geometry;
    FloatVolume* output = nullptr;
};

class FroxelDensityBatch {
public:
    // Camera160 + padding and world16 + padding, at 256-byte offsets.
    static constexpr uint64_t uniformBytesPerJob = 512;

    // Reference density shader, with a fixed 4x4x4 workgroup.
    explicit FroxelDensityBatch(const GpuContext& ctx);

    // The encoder and optional timer must belong to ctx (their owners cannot be
    // queried through WebGPU C). ctx must outlive all submitted work.
    // within this batch and cannot alias ANY input. The shared world is never changed.
    // An encode call takes an immutable camera/geometry snapshot. Multiple calls
    // before one submission are safe; the world texels are read at GPU execution.
    // Records one compute pass per job. No queue writes, submission, wait or readback.
    void encode(const GpuContext& ctx, WGPUCommandEncoder encoder,
                std::span<const FroxelDensityJob> jobs, GpuTimer* timer = nullptr);

    // Convenience wrapper: creates one encoder, records all jobs, submits once.
    void dispatch(const GpuContext& ctx, std::span<const FroxelDensityJob> jobs,
                  GpuTimer* timer = nullptr);

private:
    WGPUDevice owner_ = nullptr;
    uint64_t maxBufferSize_ = 0;
    BindGroupLayout layout_;
    PipelineLayout pipelineLayout_;
    ComputePipeline pipeline_;
};

}  // namespace ff::gpu
