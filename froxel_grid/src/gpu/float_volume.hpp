// Portable scalar density storage. No float32-filterable feature needed.
#pragma once

#include "gpu/context.hpp"
#include "gpu/verify.hpp"

#include <span>
#include <string_view>
#include <vector>

namespace ff::gpu {

class FloatVolume {
public:
    // Uses the default WebGPU 3D-texture limit: 1..256 per axis.
    FloatVolume(const GpuContext& ctx, Extent3D size, std::string_view label);
    FloatVolume(FloatVolume&&) noexcept = default;
    FloatVolume& operator=(FloatVolume&&) noexcept = default;
    FloatVolume(const FloatVolume&) = delete;
    FloatVolume& operator=(const FloatVolume&) = delete;

    // Retains a reference, plus an independent whole-volume view; never copies texels.
    // The caller guarantees texture belongs to ctx: WebGPU C cannot query its device.
    // ctx must outlive this wrapper and its submitted work. Releasing the creator's
    // reference is safe; calling wgpuTextureDestroy while in use is NOT safe.
    // Requires one mip, one sample, 3D R32Float, axes 1..256 and a binding usage.
    static FloatVolume share(const GpuContext& ctx, WGPUTexture texture, std::string_view label);

    Extent3D size() const { return size_; }
    WGPUTexture texture() const { return texture_.get(); }
    WGPUTextureView view() const { return view_.get(); }

    // Checks lifetime, known device identity and all required usage bits before a
    // GPU operation. Shared textures may have only the usages their producer needs.
    void validate(const GpuContext& ctx, WGPUTextureUsage requiredUsage = WGPUTextureUsage_None) const;

    // Dense cell-centered scalars, x fastest; input must be finite and exactly size().cells().
    void upload(const GpuContext& ctx, std::span<const float> data);
    std::vector<float> readBack(const GpuContext& ctx) const;

private:
    FloatVolume() = default;
    Extent3D size_;
    WGPUDevice owner_ = nullptr;  // borrowed identity; ctx owns the device
    WGPUTextureUsage usage_ = WGPUTextureUsage_None;
    Texture texture_;
    TextureView view_;
};

}  // namespace ff::gpu
