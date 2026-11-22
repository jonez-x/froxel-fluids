#pragma once

#include "gpu/buffer.hpp"
#include "gpu/context.hpp"
#include "gpu/handle.hpp"
#include "gpu/timer.hpp"
#include "gpu/verify.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ff::gpu {

enum class BindingKind {
    Uniform,
    ReadOnlyStorage,
    Storage,
    Texture3D,          // texture_3d<f32>, unfilterable (textureLoad)
    Texture2D,          // texture_2d<f32>, unfilterable
    StorageTexture3D,   // texture_storage_3d<r32float, write>
    StorageTexture2D,   // texture_storage_2d<r32float, write>
};

struct Binding {
    WGPUBuffer buffer = nullptr;
    uint64_t size = 0;
    WGPUTextureView view = nullptr;

    static Binding of(const GpuBuffer& b) { return {b.get(), b.size(), nullptr}; }
    static Binding of(WGPUTextureView v) { return {nullptr, 0, v}; }
};

// Workgroups needed to cover `threads` with cubic workgroups of edge `size` (z = 1 for 2D: pass z = 1).
inline Extent3D workgroups(Extent3D threads, uint32_t size = 4, uint32_t sizeZ = 4) {
    return {(threads.x + size - 1) / size, (threads.y + size - 1) / size, (threads.z + sizeZ - 1) / sizeZ};
}

class ComputeProgram {
public:
    // Binding i of group 0 has kind layout[i]. Throws std::runtime_error if the pipeline cannot be built.
    ComputeProgram(const GpuContext& ctx, std::string label, std::string_view wgsl, std::vector<BindingKind> layout,
                   std::string_view entryPoint = "main");

    const std::string& label() const { return label_; }

    // Records one compute pass into the caller's encoder. bindings.size() must equal the layout size.
    // The pass is timed as `label()` if timer is given.
    void encode(const GpuContext& ctx, WGPUCommandEncoder encoder, std::span<const Binding> bindings, Extent3D groups,
                GpuTimer* timer = nullptr) const;

private:
    std::string label_;
    std::vector<BindingKind> kinds_;
    BindGroupLayout layout_;
    PipelineLayout pipelineLayout_;
    ComputePipeline pipeline_;
};

// Creates an encoder, lets `record` fill it, and submits it.
template <class F>
void submit(const GpuContext& ctx, std::string_view label, F&& record) {
    WGPUCommandEncoderDescriptor ed{};
    ed.label = sv(label);
    CommandEncoder encoder(wgpuDeviceCreateCommandEncoder(ctx.device(), &ed));
    record(encoder.get());
    WGPUCommandBufferDescriptor cd{};
    CommandBuffer commands(wgpuCommandEncoderFinish(encoder.get(), &cd));
    const WGPUCommandBuffer list[] = {commands.get()};
    wgpuQueueSubmit(ctx.queue(), 1, list);
}

}  // namespace ff::gpu
