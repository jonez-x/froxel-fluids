#include "gpu/float_volume.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace ff::gpu {
namespace {
void validateSize(Extent3D size) {
    auto valid = [](uint32_t n) { return n >= 1 && n <= 256; };
    if (!valid(size.x) || !valid(size.y) || !valid(size.z))
        throw std::invalid_argument("float volume axes must be in [1,256]");
}

TextureView wholeView(WGPUTexture texture, std::string_view label) {
    WGPUTextureViewDescriptor vd{};
    vd.label = sv(label);
    vd.format = WGPUTextureFormat_R32Float;
    vd.dimension = WGPUTextureViewDimension_3D;
    vd.mipLevelCount = 1;
    vd.arrayLayerCount = 1;
    vd.aspect = WGPUTextureAspect_All;
    vd.usage = WGPUTextureUsage_None;  // inherit (wgpu-native v24)
    TextureView view(wgpuTextureCreateView(texture, &vd));
    if (!view) throw std::runtime_error("could not create float volume view");
    return view;
}
}  // namespace

FloatVolume::FloatVolume(const GpuContext& ctx, Extent3D size, std::string_view label)
    : size_(size), owner_(ctx.device()),
      usage_(WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding |
             WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst) {
    validateSize(size);
    WGPUTextureDescriptor td{};
    td.label = sv(label);
    td.usage = usage_;
    td.dimension = WGPUTextureDimension_3D;
    td.size = {size.x, size.y, size.z};
    td.format = WGPUTextureFormat_R32Float;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    texture_.reset(wgpuDeviceCreateTexture(ctx.device(), &td));
    if (!texture_) throw std::runtime_error("could not allocate float volume");
    view_ = wholeView(texture_.get(), label);
}

FloatVolume FloatVolume::share(const GpuContext& ctx, WGPUTexture texture, std::string_view label) {
    if (!texture) throw std::invalid_argument("cannot share a null float volume texture");
    if (wgpuTextureGetDimension(texture) != WGPUTextureDimension_3D ||
        wgpuTextureGetFormat(texture) != WGPUTextureFormat_R32Float ||
        wgpuTextureGetMipLevelCount(texture) != 1 || wgpuTextureGetSampleCount(texture) != 1)
        throw std::invalid_argument("shared float volume must be single-mip single-sample 3D R32Float");
    const Extent3D size{wgpuTextureGetWidth(texture), wgpuTextureGetHeight(texture),
                        wgpuTextureGetDepthOrArrayLayers(texture)};
    validateSize(size);
    const auto usage = wgpuTextureGetUsage(texture);
    if (!(usage & (WGPUTextureUsage_TextureBinding | WGPUTextureUsage_StorageBinding)))
        throw std::invalid_argument("shared float volume needs sampled or storage binding usage");
    FloatVolume result;
    result.size_ = size;
    result.owner_ = ctx.device();
    result.usage_ = usage;
    wgpuTextureAddRef(texture);
    result.texture_.reset(texture);
    result.view_ = wholeView(texture, label);
    return result;
}

void FloatVolume::validate(const GpuContext& ctx, WGPUTextureUsage requiredUsage) const {
    if (!texture_ || !view_) throw std::invalid_argument("float volume is empty or moved from");
    if (owner_ != ctx.device()) throw std::invalid_argument("float volume belongs to another GPU context");
    if ((usage_ & requiredUsage) != requiredUsage)
        throw std::invalid_argument("float volume lacks usage required by this operation");
}

void FloatVolume::upload(const GpuContext& ctx, std::span<const float> data) {
    validate(ctx, WGPUTextureUsage_CopyDst);
    if (data.size() != size_.cells()) throw std::invalid_argument("float volume upload size mismatch");
    if (!std::all_of(data.begin(), data.end(), [](float x) { return std::isfinite(x); }))
        throw std::invalid_argument("float volume upload contains non-finite values");
    WGPUTexelCopyTextureInfo dst{};
    dst.texture = texture_.get();
    dst.aspect = WGPUTextureAspect_All;
    WGPUTexelCopyBufferLayout layout{};
    // Queue writeTexture does not require the 256-byte stride used by buffer copies.
    layout.bytesPerRow = size_.x * sizeof(float);
    layout.rowsPerImage = size_.y;
    WGPUExtent3D extent{size_.x, size_.y, size_.z};
    wgpuQueueWriteTexture(ctx.queue(), &dst, data.data(), data.size_bytes(), &layout, &extent);
}

std::vector<float> FloatVolume::readBack(const GpuContext& ctx) const {
    validate(ctx, WGPUTextureUsage_CopySrc);
    const uint32_t bpr = paddedBytesPerRow(size_.x);
    const uint64_t bytes = uint64_t(bpr) * size_.y * size_.z;
    WGPUBufferDescriptor bd{};
    bd.label = sv("float volume readback");
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = bytes;
    Buffer buffer(wgpuDeviceCreateBuffer(ctx.device(), &bd));
    WGPUCommandEncoderDescriptor ed{};
    CommandEncoder enc(wgpuDeviceCreateCommandEncoder(ctx.device(), &ed));
    WGPUTexelCopyTextureInfo src{};
    src.texture = texture_.get();
    src.aspect = WGPUTextureAspect_All;
    WGPUTexelCopyBufferInfo dst{};
    dst.buffer = buffer.get();
    dst.layout.bytesPerRow = bpr;
    dst.layout.rowsPerImage = size_.y;
    WGPUExtent3D extent{size_.x, size_.y, size_.z};
    wgpuCommandEncoderCopyTextureToBuffer(enc.get(), &src, &dst, &extent);
    WGPUCommandBufferDescriptor cd{};
    CommandBuffer cmd(wgpuCommandEncoderFinish(enc.get(), &cd));
    const WGPUCommandBuffer commands[] = {cmd.get()};
    wgpuQueueSubmit(ctx.queue(), 1, commands);

    struct MapState { bool done = false; bool ok = false; } state;
    WGPUBufferMapCallbackInfo cb{};
    cb.mode = WGPUCallbackMode_AllowProcessEvents;
    cb.userdata1 = &state;
    cb.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* ud, void*) {
        auto& s = *static_cast<MapState*>(ud);
        s.ok = status == WGPUMapAsyncStatus_Success;
        s.done = true;
    };
    wgpuBufferMapAsync(buffer.get(), WGPUMapMode_Read, 0, bytes, cb);
    ctx.waitUntil([&] { return state.done; });
    if (!state.ok) throw std::runtime_error("float volume readback mapping failed");
    const auto* base = static_cast<const uint8_t*>(wgpuBufferGetConstMappedRange(buffer.get(), 0, bytes));
    if (!base) throw std::runtime_error("float volume readback range unavailable");
    std::vector<float> out(size_.cells());
    for (uint32_t z = 0; z < size_.z; ++z)
        for (uint32_t y = 0; y < size_.y; ++y)
            std::memcpy(&out[cellIndex(size_, 0, y, z)], base + (uint64_t(z) * size_.y + y) * bpr,
                        std::size_t(size_.x) * sizeof(float));
    wgpuBufferUnmap(buffer.get());
    return out;
}

}  // namespace ff::gpu
