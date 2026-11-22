#pragma once

#include "gpu/context.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ff::gpu {

struct PassResult {
    std::string name;
    std::optional<double> gpuMs;           // set only when the tick period is known
    std::optional<uint64_t> elapsedTicks;  // native timestamp difference, when available
};

class GpuTimer {
public:
    // Uses FF_GPU_TIMESTAMP_PERIOD_NS when set to a finite, positive ns/tick value
    // verified for the selected native adapter/backend. Otherwise reports raw ticks.
    GpuTimer(const GpuContext& ctx, uint32_t maxPasses);
    // Explicit per-device configuration overrides the environment. nullopt requests raw ticks only.
    GpuTimer(const GpuContext& ctx, uint32_t maxPasses, std::optional<double> timestampPeriodNs);

    // Registers the next pass. Returns the timestamp writes for its compute pass descriptor,
    // or nullptr if timing is unavailable or maxPasses is exhausted.
    const WGPUComputePassTimestampWrites* beginPass(const std::string& name);

    // Resolves all registered passes. A pass beyond capacity has neither ticks nor milliseconds.
    std::vector<PassResult> collect(const GpuContext& ctx);

private:
    bool enabled_ = false;
    uint32_t maxPasses_;
    std::optional<double> timestampPeriodNs_;
    QuerySet querySet_;
    Buffer resolveBuffer_, readBuffer_;
    std::vector<std::string> names_;
    std::vector<WGPUComputePassTimestampWrites> writes_;  // reserved, so pointers stay valid
};

}  // namespace ff::gpu
