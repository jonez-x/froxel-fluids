// RAII wrapper for WebGPU C handles (wgpu-native / Dawn via webgpu.h).
#pragma once

#include <webgpu/webgpu.h>

#include <string>
#include <string_view>
#include <utility>

namespace ff::gpu {

// Move-only owner of a WebGPU handle; calls the matching wgpu*Release on destruction.
template <typename T, void (*Release)(T)>
class Handle {
public:
    Handle() = default;
    explicit Handle(T h) : h_(h) {}
    ~Handle() { reset(); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& o) noexcept : h_(std::exchange(o.h_, nullptr)) {}
    Handle& operator=(Handle&& o) noexcept {
        if (this != &o) {
            reset();
            h_ = std::exchange(o.h_, nullptr);
        }
        return *this;
    }
    void reset(T h = nullptr) {
        if (h_) Release(h_);
        h_ = h;
    }
    T get() const { return h_; }
    explicit operator bool() const { return h_ != nullptr; }

private:
    T h_ = nullptr;
};

using Instance = Handle<WGPUInstance, wgpuInstanceRelease>;
using Adapter = Handle<WGPUAdapter, wgpuAdapterRelease>;
using Device = Handle<WGPUDevice, wgpuDeviceRelease>;
using Queue = Handle<WGPUQueue, wgpuQueueRelease>;
using Texture = Handle<WGPUTexture, wgpuTextureRelease>;
using TextureView = Handle<WGPUTextureView, wgpuTextureViewRelease>;
using Buffer = Handle<WGPUBuffer, wgpuBufferRelease>;
using ShaderModule = Handle<WGPUShaderModule, wgpuShaderModuleRelease>;
using BindGroupLayout = Handle<WGPUBindGroupLayout, wgpuBindGroupLayoutRelease>;
using PipelineLayout = Handle<WGPUPipelineLayout, wgpuPipelineLayoutRelease>;
using ComputePipeline = Handle<WGPUComputePipeline, wgpuComputePipelineRelease>;
using BindGroup = Handle<WGPUBindGroup, wgpuBindGroupRelease>;
using QuerySet = Handle<WGPUQuerySet, wgpuQuerySetRelease>;
using CommandEncoder = Handle<WGPUCommandEncoder, wgpuCommandEncoderRelease>;
using CommandBuffer = Handle<WGPUCommandBuffer, wgpuCommandBufferRelease>;
using ComputePassEncoder = Handle<WGPUComputePassEncoder, wgpuComputePassEncoderRelease>;

// String helpers for WGPUStringView.
inline WGPUStringView sv(std::string_view s) { return WGPUStringView{s.data(), s.size()}; }
inline std::string str(WGPUStringView v) {
    if (!v.data) return {};
    return v.length == WGPU_STRLEN ? std::string(v.data) : std::string(v.data, v.length);
}

}  // namespace ff::gpu
