 // Volume dimensions and memory layout shared by the WebGPU modules.
#pragma once

#include <cstdint>

namespace ff::gpu {

struct Extent3D {
    uint32_t x = 1, y = 1, z = 1;
    uint64_t cells() const { return uint64_t(x) * y * z; }
};

constexpr uint64_t cellIndex(Extent3D s, uint32_t x, uint32_t y, uint32_t z) {
    return x + uint64_t(s.x) * (y + uint64_t(s.y) * z);
}

// WebGPU requires bytesPerRow to be a multiple of 256 for texture <-> buffer copies.
constexpr uint32_t paddedBytesPerRow(uint32_t width, uint32_t bytesPerTexel = 4) {
    const uint32_t raw = width * bytesPerTexel;
    return (raw + 255u) / 256u * 256u;
}

}  // namespace ff::gpu
