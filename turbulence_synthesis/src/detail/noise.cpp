#include "detail/noise.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace ff::detail {

namespace {
// The 12 edge directions of a cube (classic improved Perlin noise).
constexpr double kGrad[12][3] = {{1, 1, 0}, {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0}, {1, 0, 1}, {-1, 0, 1},
                                 {1, 0, -1}, {-1, 0, -1}, {0, 1, 1}, {0, -1, 1}, {0, 1, -1}, {0, -1, -1}};
}  // namespace

GradientNoise::GradientNoise(uint32_t seed) {
    std::array<uint8_t, 256> p{};
    std::iota(p.begin(), p.end(), uint8_t{0});
    std::mt19937 rng(seed);
    std::shuffle(p.begin(), p.end(), rng);
    for (int i = 0; i < 512; ++i) perm_[i] = p[i & 255];
}

glm::dvec3 GradientNoise::grad(int x, int y, int z) const {
    const int h = perm_[(perm_[(perm_[x & 255] + y) & 255] + z) & 255] % 12;
    return {kGrad[h][0], kGrad[h][1], kGrad[h][2]};
}

GradientNoise::Sample GradientNoise::eval(glm::dvec3 p) const {
    const glm::dvec3 fl = glm::floor(p);
    const int ix = int(fl.x), iy = int(fl.y), iz = int(fl.z);
    const glm::dvec3 f = p - fl;
    // quintic fade u(f) = 6f^5 - 15f^4 + 10f^3 and its derivative du = 30 f^2 (f - 1)^2
    const glm::dvec3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    const glm::dvec3 du = 30.0 * f * f * (f * (f - 2.0) + 1.0);

    const glm::dvec3 ga = grad(ix, iy, iz), gb = grad(ix + 1, iy, iz), gc = grad(ix, iy + 1, iz),
                     gd = grad(ix + 1, iy + 1, iz), ge = grad(ix, iy, iz + 1), gf = grad(ix + 1, iy, iz + 1),
                     gg = grad(ix, iy + 1, iz + 1), gh = grad(ix + 1, iy + 1, iz + 1);
    const double va = glm::dot(ga, f), vb = glm::dot(gb, f - glm::dvec3(1, 0, 0)),
                 vc = glm::dot(gc, f - glm::dvec3(0, 1, 0)), vd = glm::dot(gd, f - glm::dvec3(1, 1, 0)),
                 ve = glm::dot(ge, f - glm::dvec3(0, 0, 1)), vf = glm::dot(gf, f - glm::dvec3(1, 0, 1)),
                 vg = glm::dot(gg, f - glm::dvec3(0, 1, 1)), vh = glm::dot(gh, f - glm::dvec3(1, 1, 1));

    // Trilinear blend of the corner values with weights u (Quilez' analytic-derivative form).
    const double k1 = vb - va, k2 = vc - va, k3 = ve - va, k4 = va - vb - vc + vd, k5 = va - vc - ve + vg,
                 k6 = va - vb - ve + vf, k7 = -va + vb + vc - vd + ve - vf - vg + vh;
    Sample s;
    s.value = va + u.x * k1 + u.y * k2 + u.z * k3 + u.x * u.y * k4 + u.y * u.z * k5 + u.z * u.x * k6 +
              u.x * u.y * u.z * k7;
    s.gradient = ga + u.x * (gb - ga) + u.y * (gc - ga) + u.z * (ge - ga) + u.x * u.y * (ga - gb - gc + gd) +
                 u.y * u.z * (ga - gc - ge + gg) + u.z * u.x * (ga - gb - ge + gf) +
                 u.x * u.y * u.z * (-ga + gb + gc - gd + ge - gf - gg + gh) +
                 du * glm::dvec3(k1 + u.y * k4 + u.z * k6 + u.y * u.z * k7,  //
                                 k2 + u.z * k5 + u.x * k4 + u.z * u.x * k7,  //
                                 k3 + u.x * k6 + u.y * k5 + u.x * u.y * k7);
    return s;
}

}  // namespace ff::detail
