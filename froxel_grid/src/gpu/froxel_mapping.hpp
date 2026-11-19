// Binding-free camera mapping functions for W2 and downstream compute kernels.
#pragma once

#include <string>
#include <string_view>

namespace ff::gpu {

// Prepend the shared CPU-compatible camera mapping WGSL to a caller's kernel.
std::string withFroxelMapping(std::string_view kernel);

}  // namespace ff::gpu
