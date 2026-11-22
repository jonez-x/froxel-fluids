#include "gpu/timer.hpp"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace ff::gpu {
namespace {
std::optional<double> configuredTimestampPeriod() {
    const char* value = std::getenv("FF_GPU_TIMESTAMP_PERIOD_NS");
    if (!value) return std::nullopt;
    char* end = nullptr;
    errno = 0;
    const double period = std::strtod(value, &end);
    if (end == value || *end != '\0' || errno == ERANGE || !std::isfinite(period) || period <= 0.0)
        return std::nullopt;
    return period;
}
}  // namespace

GpuTimer::GpuTimer(const GpuContext& ctx, uint32_t maxPasses)
    : GpuTimer(ctx, maxPasses, configuredTimestampPeriod()) {}

GpuTimer::GpuTimer(const GpuContext& ctx, uint32_t maxPasses, std::optional<double> timestampPeriodNs)
    : enabled_(ctx.hasTimestamps()), maxPasses_(maxPasses), timestampPeriodNs_(timestampPeriodNs) {
    if (timestampPeriodNs_ && (!std::isfinite(*timestampPeriodNs_) || *timestampPeriodNs_ <= 0.0))
        throw std::invalid_argument("GPU timestamp period must be finite and positive (ns per tick)");
    writes_.reserve(maxPasses);
    if (!enabled_ || maxPasses == 0) return;
    WGPUQuerySetDescriptor qd{};
    qd.label = sv("pass timestamps");
    qd.type = WGPUQueryType_Timestamp;
    qd.count = 2 * maxPasses;
    querySet_.reset(wgpuDeviceCreateQuerySet(ctx.device(), &qd));

    const uint64_t bytes = uint64_t(2) * maxPasses * sizeof(uint64_t);
    WGPUBufferDescriptor rd{};
    rd.label = sv("timestamp resolve");
    rd.usage = WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc;
    rd.size = bytes;
    resolveBuffer_.reset(wgpuDeviceCreateBuffer(ctx.device(), &rd));
    WGPUBufferDescriptor md{};
    md.label = sv("timestamp readback");
    md.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    md.size = bytes;
    readBuffer_.reset(wgpuDeviceCreateBuffer(ctx.device(), &md));
}

const WGPUComputePassTimestampWrites* GpuTimer::beginPass(const std::string& name) {
    names_.push_back(name);
    if (!enabled_ || writes_.size() >= maxPasses_) return nullptr;
    const auto i = static_cast<uint32_t>(writes_.size());
    writes_.push_back({querySet_.get(), 2 * i, 2 * i + 1});
    return &writes_.back();
}

std::vector<PassResult> GpuTimer::collect(const GpuContext& ctx) {
    std::vector<PassResult> out;
    for (const auto& n : names_) out.push_back({n, std::nullopt, std::nullopt});
    if (!enabled_ || writes_.empty()) return out;

    const auto count = static_cast<uint32_t>(2 * writes_.size());
    const uint64_t bytes = uint64_t(count) * sizeof(uint64_t);
    WGPUCommandEncoderDescriptor ed{};
    CommandEncoder enc(wgpuDeviceCreateCommandEncoder(ctx.device(), &ed));
    wgpuCommandEncoderResolveQuerySet(enc.get(), querySet_.get(), 0, count, resolveBuffer_.get(), 0);
    wgpuCommandEncoderCopyBufferToBuffer(enc.get(), resolveBuffer_.get(), 0, readBuffer_.get(), 0, bytes);
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
    wgpuBufferMapAsync(readBuffer_.get(), WGPUMapMode_Read, 0, bytes, mcb);
    ctx.waitUntil([&] { return st.done; });
    if (!st.ok) return out;

    const void* mapped = wgpuBufferGetConstMappedRange(readBuffer_.get(), 0, bytes);
    if (!mapped) {
        wgpuBufferUnmap(readBuffer_.get());
        return out;
    }
    std::vector<uint64_t> ticks(count);
    std::memcpy(ticks.data(), mapped, bytes);
    wgpuBufferUnmap(readBuffer_.get());
    for (std::size_t i = 0; i < writes_.size(); ++i) {
        const uint64_t a = ticks[2 * i], b = ticks[2 * i + 1];
        if (b < a) continue;
        out[i].elapsedTicks = b - a;
        if (timestampPeriodNs_) {
            const double ms = (double(b - a) / 1.0e6) * *timestampPeriodNs_;
            if (std::isfinite(ms)) out[i].gpuMs = ms;
        }
    }
    return out;
}

}  // namespace ff::gpu
