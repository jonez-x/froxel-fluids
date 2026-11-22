#pragma once

#include "gpu/handle.hpp"

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace ff::gpu {

struct AdapterInfo {
    std::string name;     // device name, e.g. "NVIDIA GeForce RTX 3070"
    std::string vendor;
    std::string backend;  // Vulkan, Metal, D3D12, ...
    std::string type;     // discrete GPU, integrated GPU, CPU, unknown
};

// Thrown by GpuContext::create() when no usable adapter or device exists.
class NoAdapterError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class GpuContext {
public:
    // High-performance adapter; enables timestamp queries if the adapter supports them.
    static GpuContext create();

    GpuContext(GpuContext&&) noexcept;
    GpuContext& operator=(GpuContext&&) noexcept;
    ~GpuContext();

    const AdapterInfo& info() const;
    bool hasTimestamps() const;
    const std::vector<std::string>& errors() const;

    WGPUInstance instance() const;
    WGPUDevice device() const;
    WGPUQueue queue() const;

    // Drives asynchronous WebGPU callbacks (map, request) until done() returns true.
    void waitUntil(const std::function<bool()>& done) const;

private:
    struct Impl;
    explicit GpuContext(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

}  // namespace ff::gpu
