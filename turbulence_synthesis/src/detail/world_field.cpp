#include "detail/world_field.hpp"

#include <algorithm>
#include <cmath>

namespace ff::detail {

WorldField::WorldField(uint32_t n, double h, glm::dvec3 origin)
    : n_(n), h_(h), origin_(origin), vel_(uint64_t(n) * n * n, glm::dvec3(0.0)), rho_(uint64_t(n) * n * n, 0.0),
      vortMag_(uint64_t(n) * n * n, 0.0) {}

bool WorldField::inside(glm::dvec3 x) const {
    const glm::dvec3 g = (x - origin_) / h_;
    return g.x >= 0 && g.y >= 0 && g.z >= 0 && g.x <= n_ && g.y <= n_ && g.z <= n_;
}

glm::dvec3 WorldField::cellCenter(uint32_t i, uint32_t j, uint32_t k) const {
    return origin_ + h_ * glm::dvec3(i + 0.5, j + 0.5, k + 0.5);
}

void WorldField::computeVorticity() {
    const int n = int(n_);
    auto v = [&](int i, int j, int k) {
        i = std::clamp(i, 0, n - 1);
        j = std::clamp(j, 0, n - 1);
        k = std::clamp(k, 0, n - 1);
        return vel_[index(i, j, k)];
    };
    for (int k = 0; k < n; ++k)
        for (int j = 0; j < n; ++j)
            for (int i = 0; i < n; ++i) {
                const glm::dvec3 dx = (v(i + 1, j, k) - v(i - 1, j, k)) / (2 * h_);
                const glm::dvec3 dy = (v(i, j + 1, k) - v(i, j - 1, k)) / (2 * h_);
                const glm::dvec3 dz = (v(i, j, k + 1) - v(i, j, k - 1)) / (2 * h_);
                const glm::dvec3 w(dy.z - dz.y, dz.x - dx.z, dx.y - dy.x);
                vortMag_[index(i, j, k)] = glm::length(w);
            }
}

template <typename T>
T WorldField::trilinear(const std::vector<T>& f, glm::dvec3 x) const {
    if (!inside(x)) return T(0);
    const glm::dvec3 g = glm::clamp((x - origin_) / h_ - 0.5, glm::dvec3(0.0), glm::dvec3(n_ - 1.000001));
    const int i = int(g.x), j = int(g.y), k = int(g.z);
    const glm::dvec3 t = g - glm::dvec3(i, j, k);
    const int i1 = std::min(i + 1, int(n_) - 1), j1 = std::min(j + 1, int(n_) - 1), k1 = std::min(k + 1, int(n_) - 1);
    auto at = [&](int a, int b, int c) { return f[index(a, b, c)]; };
    const T c00 = at(i, j, k) * (1 - t.x) + at(i1, j, k) * t.x;
    const T c10 = at(i, j1, k) * (1 - t.x) + at(i1, j1, k) * t.x;
    const T c01 = at(i, j, k1) * (1 - t.x) + at(i1, j, k1) * t.x;
    const T c11 = at(i, j1, k1) * (1 - t.x) + at(i1, j1, k1) * t.x;
    const T c0 = c00 * (1 - t.y) + c10 * t.y;
    const T c1 = c01 * (1 - t.y) + c11 * t.y;
    return c0 * (1 - t.z) + c1 * t.z;
}

glm::dvec3 WorldField::sampleVelocity(glm::dvec3 x) const { return trilinear(vel_, x); }
double WorldField::sampleDensity(glm::dvec3 x) const { return trilinear(rho_, x); }

WorldField::Smooth WorldField::sampleVorticitySmooth(glm::dvec3 x) const {
    Smooth s;
    if (!inside(x)) return s;
    const glm::dvec3 g = (x - origin_) / h_ - 0.5;  // grid coordinates of cell centers
    const glm::ivec3 c(int(std::lround(g.x)), int(std::lround(g.y)), int(std::lround(g.z)));
    const glm::dvec3 t = g - glm::dvec3(c);  // in [-0.5, 0.5]
    double w[3][3], dw[3][3];               // quadratic B-spline weights and derivatives per axis
    for (int a = 0; a < 3; ++a) {
        const double ta = t[a];
        w[a][0] = 0.5 * (0.5 - ta) * (0.5 - ta);
        w[a][1] = 0.75 - ta * ta;
        w[a][2] = 0.5 * (0.5 + ta) * (0.5 + ta);
        dw[a][0] = -(0.5 - ta);
        dw[a][1] = -2.0 * ta;
        dw[a][2] = (0.5 + ta);
    }
    const int n = int(n_);
    for (int dk = 0; dk < 3; ++dk)
        for (int dj = 0; dj < 3; ++dj)
            for (int di = 0; di < 3; ++di) {
                const int i = std::clamp(c.x + di - 1, 0, n - 1), j = std::clamp(c.y + dj - 1, 0, n - 1),
                          k = std::clamp(c.z + dk - 1, 0, n - 1);
                const double f = vortMag_[index(i, j, k)];
                s.value += f * w[0][di] * w[1][dj] * w[2][dk];
                s.gradient += f * glm::dvec3(dw[0][di] * w[1][dj] * w[2][dk], w[0][di] * dw[1][dj] * w[2][dk],
                                             w[0][di] * w[1][dj] * dw[2][dk]);
            }
    s.gradient /= h_;  // grid units -> world units
    return s;
}

WorldField makePlumeScene() {
    const uint32_t n = 32;
    const double h = 0.5;
    WorldField w(n, h, {-8.0, 0.0, -8.0});
    const double R = 2.0, U = 1.5, S = 1.0, Rrho = 1.8;
    for (uint32_t k = 0; k < n; ++k)
        for (uint32_t j = 0; j < n; ++j)
            for (uint32_t i = 0; i < n; ++i) {
                const glm::dvec3 p = w.cellCenter(i, j, k);
                const double r2 = p.x * p.x + p.z * p.z, r = std::sqrt(r2);
                const double fadeIn = 1.0 - std::exp(-p.y / 1.5);  // plume speeds up above the ground
                const double g = std::exp(-r2 / (R * R)) * fadeIn;
                const double vy = U * g;
                const double vt = S * (r / R) * g;  // swirl around the axis
                const glm::dvec3 tangent = r > 1e-9 ? glm::dvec3(-p.z / r, 0.0, p.x / r) : glm::dvec3(0.0);
                w.velocityAt(i, j, k) = glm::dvec3(0.0, vy, 0.0) + vt * tangent;
                // compact smoke column with a soft edge: (1 - r^2/Rrho^2)^2 inside, 0 outside
                const double sr = r2 / (Rrho * Rrho);
                const double column = sr < 1.0 ? (1.0 - sr) * (1.0 - sr) : 0.0;
                w.densityAt(i, j, k) = column * std::exp(-p.y / 10.0) * (1.0 - std::exp(-p.y / 0.5));
            }
    w.computeVorticity();
    return w;
}

WorldField makeRotationScene(double omega) {
    const uint32_t n = 32;
    WorldField w(n, 0.5, {-8.0, 0.0, -8.0});
    for (uint32_t k = 0; k < n; ++k)
        for (uint32_t j = 0; j < n; ++j)
            for (uint32_t i = 0; i < n; ++i) {
                const glm::dvec3 p = w.cellCenter(i, j, k);
                w.velocityAt(i, j, k) = omega * glm::dvec3(-p.z, 0.0, p.x);
                w.densityAt(i, j, k) = 1.0;
            }
    w.computeVorticity();
    return w;
}

}  // namespace ff::detail
