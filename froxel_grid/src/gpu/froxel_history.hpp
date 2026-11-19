// Geometric history WGSL for downstream kernels; physical transport is caller-owned.
#pragma once

#include <string>
#include <string_view>

namespace ff::gpu {

// Prepend camera mapping and geometric history helpers to a caller's kernel.
std::string withFroxelHistory(std::string_view kernel);

}  // namespace ff::gpu
