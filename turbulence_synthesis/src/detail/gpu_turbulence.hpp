// Turbulence synthesis on the GPU (spec 004, contracts/gpu-detail-api.md): fills the per-camera
// froxel detail buffer (contracts/froxel-detail-buffer.md) with shaders/turbulence.wgsl.
// Library ff_detail_gpu; the CPU reference (spec 003) is the test oracle.
#pragma once

#include "froxel_link/froxel_link.hpp"
#include "detail/obstacles.hpp"
#include "detail/froxel_field.hpp"
#include "detail/turbulence.hpp"
#include "detail/world_field.hpp"
#include "gpu/buffer.hpp"
#include "gpu/context.hpp"
#include "gpu/timer.hpp"

#include <memory>

namespace ff::detail {

// One per world field and settings (rule, strength, seed, maxLayers). Uploads |omega| (f32, n^3) and
// the noise permutation once; shared by all cameras. Reads the world, never writes it (constitution III).
// Obstacles (spec 009): any detail::Obstacles with a GPU form (Obstacles::gpuData), uploaded once; may be null.
// Throws std::invalid_argument for moving or Custom shapes. obstacles must outlive the object.
class GpuTurbulence {
public:
    GpuTurbulence(const gpu::GpuContext& ctx, const WorldField& world, DetailSettings settings,
                  const Obstacles* obstacles = nullptr);
    ~GpuTurbulence();
    GpuTurbulence(const GpuTurbulence&) = delete;
    GpuTurbulence& operator=(const GpuTurbulence&) = delete;

    const DetailSettings& settings() const;
    const TurbulenceSynthesis& reference() const;  // the CPU synthesis with the same settings

    struct Impl;
    const Impl& impl() const { return *impl_; }

private:
    std::unique_ptr<Impl> impl_;
};

// The detail buffer of one camera, plus its params and slice table. Overwritten on every synthesize.
class FroxelDetail {
public:
    FroxelDetail(const gpu::GpuContext& ctx, const camera::FroxelGrid& grid);
    ~FroxelDetail();
    FroxelDetail(const FroxelDetail&) = delete;
    FroxelDetail& operator=(const FroxelDetail&) = delete;

    const gpu::GpuBuffer& buffer() const;  // vec4<f32> per froxel
    const camera::FroxelGrid& grid() const;

    // One compute pass "turbulence": every froxel of this camera. timer may be null.
    void synthesize(const GpuTurbulence& t, const camera::Camera& cam, gpu::GpuTimer* timer = nullptr);

    // Blocking read-back into a CPU field with the same layout.
    FroxelDetailField readBack(const camera::Camera& cam) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ff::detail
