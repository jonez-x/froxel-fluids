#pragma once

#include "gpu/context.hpp"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

namespace ff::gpu {

class GpuBuffer {
public:
    // size is rounded up to a multiple of 16 bytes (minimum 16). usage: WGPUBufferUsage_* flags;
    // CopyDst and CopySrc are always added so that write() and read() work.
    GpuBuffer(const GpuContext& ctx, uint64_t bytes, WGPUBufferUsage usage, std::string_view label);

    WGPUBuffer get() const { return buffer_.get(); }
    uint64_t size() const { return size_; }

    // For a nonempty write both offset and bytes must be multiples of four.
    void write(const GpuContext& ctx, const void* data, uint64_t bytes, uint64_t offset = 0) const;

    // Copies the first `bytes` bytes to the CPU. Blocks until done.
    std::vector<uint8_t> read(const GpuContext& ctx, uint64_t bytes) const;

    template <class T>
    std::vector<T> readAs(const GpuContext& ctx, size_t count) const {
        static_assert(std::is_trivially_copyable_v<T>);
        if (count > size_ / sizeof(T)) throw std::out_of_range("GpuBuffer::readAs past the end");
        const std::vector<uint8_t> raw = read(ctx, count * sizeof(T));
        std::vector<T> out(count);
        if (count) std::memcpy(out.data(), raw.data(), count * sizeof(T));
        return out;
    }

private:
    uint64_t size_;
    Buffer buffer_;
};

}  // namespace ff::gpu
