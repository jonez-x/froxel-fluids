// 3D gradient (Perlin) noise with an analytic gradient (spec 003, research R1).
// Quintic fade, so value and gradient are continuous. Values are roughly in [-1, 1].
#pragma once

#include <glm/glm.hpp>

#include <array>
#include <cstdint>

namespace ff::detail {

class GradientNoise {
public:
    explicit GradientNoise(uint32_t seed = 1);

    struct Sample {
        double value = 0.0;
        glm::dvec3 gradient{0.0};
    };

    // Noise value and its exact gradient at p.
    Sample eval(glm::dvec3 p) const;

    // Permutation table (256 entries repeated twice), uploaded as is for the GPU version (spec 004).
    const std::array<uint8_t, 512>& permutation() const { return perm_; }

private:
    glm::dvec3 grad(int x, int y, int z) const;
    std::array<uint8_t, 512> perm_{};
};

}  // namespace ff::detail
