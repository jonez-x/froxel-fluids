#include "gpu/buffer.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace ff::gpu {

GpuBuffer::GpuBuffer(const GpuContext& ctx, uint64_t bytes, WGPUBufferUsage usage, std::string_view label) {
    if (bytes > std::numeric_limits<uint64_t>::max() - 15)
        throw std::length_error("GPU buffer size overflows 16-byte alignment");
    size_ = std::max<uint64_t>(16, (bytes + 15) / 16 * 16);
    WGPUBufferDescriptor bd{};
    bd.label = sv(label);
    bd.usage = usage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    bd.size = size_;
    buffer_.reset(wgpuDeviceCreateBuffer(ctx.device(), &bd));
    if (!buffer_) throw std::runtime_error("cannot create GPU buffer " + std::string(label));
}

void GpuBuffer::write(const GpuContext& ctx, const void* data, uint64_t bytes, uint64_t offset) const {
    if (bytes == 0) return;
    if (offset > size_ || bytes > size_ - offset) throw std::out_of_range("GpuBuffer::write past the end");
    if (!data || offset % 4 != 0 || bytes % 4 != 0)
        throw std::invalid_argument("GpuBuffer::write needs data and a four-byte-aligned offset and length");
    wgpuQueueWriteBuffer(ctx.queue(), buffer_.get(), offset, data, size_t(bytes));
}

std::vector<uint8_t> GpuBuffer::read(const GpuContext& ctx, uint64_t bytes) const {
    if (bytes > size_) throw std::out_of_range("GpuBuffer::read past the end");
    if (bytes == 0) return {};
    const uint64_t copy = (bytes + 3) / 4 * 4;
    WGPUBufferDescriptor bd{};
    bd.label = sv("readback");
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = copy;
    Buffer staging(wgpuDeviceCreateBuffer(ctx.device(), &bd));

    WGPUCommandEncoderDescriptor ed{};
    CommandEncoder enc(wgpuDeviceCreateCommandEncoder(ctx.device(), &ed));
    wgpuCommandEncoderCopyBufferToBuffer(enc.get(), buffer_.get(), 0, staging.get(), 0, copy);
    WGPUCommandBufferDescriptor cd{};
    CommandBuffer cmd(wgpuCommandEncoderFinish(enc.get(), &cd));
    WGPUCommandBuffer cmds[] = {cmd.get()};
    wgpuQueueSubmit(ctx.queue(), 1, cmds);

    struct MapState {
        bool done = false;
        bool ok = false;
    } st;
    WGPUBufferMapCallbackInfo mcb{};
    mcb.mode = WGPUCallbackMode_AllowProcessEvents;
    mcb.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* ud1, void*) {
        auto* s = static_cast<MapState*>(ud1);
        s->ok = status == WGPUMapAsyncStatus_Success;
        s->done = true;
    };
    mcb.userdata1 = &st;
    wgpuBufferMapAsync(staging.get(), WGPUMapMode_Read, 0, copy, mcb);
    ctx.waitUntil([&] { return st.done; });
    if (!st.ok) throw std::runtime_error("buffer mapping failed during read-back");

    std::vector<uint8_t> out(bytes);
    const auto* base = static_cast<const uint8_t*>(wgpuBufferGetConstMappedRange(staging.get(), 0, copy));
    if (!base) throw std::runtime_error("GPU buffer readback range unavailable");
    std::memcpy(out.data(), base, bytes);
    wgpuBufferUnmap(staging.get());
    return out;
}

}  // namespace ff::gpu
