#include "detail/turbulence.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace ff::detail {

namespace {
// Decorrelates the three potential channels and the layers (arbitrary large offsets in noise space).
constexpr double kChannelOffset[3][3] = {{0.0, 0.0, 0.0}, {31.41, 17.73, 59.27}, {-73.13, 11.31, 23.97}};

// Wall ramp in t = y / d0: smoothstep, so value AND slope are zero at the wall (no-slip: zero velocity).
// (Bridson 2007's ramp has a nonzero slope at t = 0, which keeps only the normal component zero.)
void ramp(double t, double& r, double& dr) {
    if (t <= 0.0) { r = 0.0; dr = 0.0; return; }
    if (t >= 1.0) { r = 1.0; dr = 0.0; return; }
    r = t * t * (3.0 - 2.0 * t);
    dr = 6.0 * t * (1.0 - t);
}

// Slip constraint (spec 009, same formulas as curl/boundary.cpp of spec 005/006): Bridson 2007 eq. 5
// with the signed ramp of eq. 4, blended to the scalar fade (eq. 3) near the medial axis between two
// obstacles (gap < width).
double rampSlip(double r) {
    if (r >= 1.0) return 1.0;
    if (r <= -1.0) return -1.0;
    const double r2 = r * r;
    return r * (15.0 / 8.0 - r2 * (10.0 / 8.0 - r2 * (3.0 / 8.0)));
}
double smoothstep01(double r) {
    if (r <= 0.0) return 0.0;
    if (r >= 1.0) return 1.0;
    return r * r * (3.0 - 2.0 * r);
}
glm::dvec3 constrainSlip(glm::dvec3 psi, const ObstacleQuery& q, double width) {
    if (q.distance >= width) return psi;
    const double alpha = rampSlip(q.distance / width);
    const glm::dvec3 tangentialFade = alpha * psi + (1.0 - alpha) * q.normal * glm::dot(q.normal, psi);
    const double beta = smoothstep01(q.gap / width);
    if (beta >= 1.0) return tangentialFade;
    return beta * tangentialFade + (1.0 - beta) * (alpha * psi);
}
}  // namespace

LayerConstants layerConstants(double cellSize, int layer) {
    LayerConstants c;
    c.frequency = std::ldexp(1.0, layer) / cellSize;  // 2^i / h
    c.amplitude = std::pow(2.0, -5.0 / 6.0 * double(layer - 1));
    c.potentialAmp = c.amplitude / c.frequency;
    const glm::dvec3 layerOffset(7.13 * layer, 3.71 * layer, 5.29 * layer);
    for (int ch = 0; ch < 3; ++ch)
        c.offset[ch] = glm::dvec3(kChannelOffset[ch][0], kChannelOffset[ch][1], kChannelOffset[ch][2]) + layerOffset;
    return c;
}

TurbulenceSynthesis::TurbulenceSynthesis(const WorldField& world, DetailSettings settings, const Obstacles* obstacles)
    : world_(world), s_(settings), noise_(settings.seed),
      d0_(settings.wallDistance > 0.0 ? settings.wallDistance : world.cellSize()), obstacles_(obstacles) {}

int TurbulenceSynthesis::layersForSlice(const camera::Camera& cam, const camera::FroxelGrid& grid, uint32_t k) const {
    const auto measure =
        s_.rule == LayerRule::OnScreen ? froxel_link::SizeMeasure::OnScreen : froxel_link::SizeMeasure::LargestEdge;
    const float size = froxel_link::sliceFroxelSize(cam, grid, k, measure);
    return std::min(s_.maxLayers, froxel_link::detailLayers(size, float(world_.cellSize())));
}

int TurbulenceSynthesis::layersFor(const camera::Camera& cam, const camera::FroxelGrid& grid, glm::dvec3 x) const {
    const std::optional<uint32_t> k = froxel_link::sliceAt(cam, grid, glm::vec3(x));
    return k ? layersForSlice(cam, grid, *k) : 0;
}

DetailSample TurbulenceSynthesis::sample(const camera::Camera& cam, const camera::FroxelGrid& grid,
                                         glm::dvec3 x) const {
    DetailSample d;
    d.layers = layersFor(cam, grid, x);
    if (d.layers > 0) d.velocity = velocity(x, 1, d.layers);
    return d;
}

glm::dvec3 TurbulenceSynthesis::velocity(glm::dvec3 x, int firstLayer, int lastLayer) const {
    if (lastLayer < firstLayer || x.y <= 0.0 || !world_.inside(x)) return glm::dvec3(0.0);
    const double dObstacle = obstacles_ ? obstacles_->distance(x) : std::numeric_limits<double>::infinity();
    if (dObstacle < 0.0) return glm::dvec3(0.0);  // inside an obstacle: no smoke, no detail (spec 009)

    // Amplitude A = strength * h * |omega| * ramp(y / d0), with its exact gradient.
    const WorldField::Smooth w = world_.sampleVorticitySmooth(x);
    double r, dr;
    ramp(x.y / d0_, r, dr);
    const double c = s_.strength * world_.cellSize();
    const double A = c * w.value * r;
    const glm::dvec3 gradA = c * (w.gradient * r + glm::dvec3(0.0, w.value * dr / d0_, 0.0));
    if (A == 0.0 && gradA == glm::dvec3(0.0)) return glm::dvec3(0.0);  // still air or at the wall

    glm::dvec3 u(0.0);
    for (int i = firstLayer; i <= lastLayer; ++i) {
        const LayerConstants lc = layerConstants(world_.cellSize(), i);
        const double f = lc.frequency, a = lc.amplitude;
        if (dObstacle < s_.obstacleFade / f) {  // inside this layer's obstacle zone (spec 009)
            u += constrainedLayer(x, lc);
            continue;
        }
        GradientNoise::Sample n[3];
        for (int ch = 0; ch < 3; ++ch) n[ch] = noise_.eval(f * x + lc.offset[ch]);
        // psi = (a / f) * (n0, n1, n2); grad(psi_c) = a * grad(n_c) (chain rule cancels f).
        const glm::dvec3 psi = (a / f) * glm::dvec3(n[0].value, n[1].value, n[2].value);
        const glm::dvec3 g0 = a * n[0].gradient, g1 = a * n[1].gradient, g2 = a * n[2].gradient;
        const glm::dvec3 curlPsi(g2.y - g1.z, g0.z - g2.x, g1.x - g0.y);
        u += A * curlPsi + glm::cross(gradA, psi);  // curl(A psi) = A curl(psi) + grad(A) x psi
    }
    return u;
}

// A(x) = strength * h * |omega|(x) * ramp(y / d0), value only.
double TurbulenceSynthesis::amplitudeAt(glm::dvec3 x) const {
    double r, dr;
    ramp(x.y / d0_, r, dr);
    return s_.strength * world_.cellSize() * world_.sampleVorticitySmooth(x).value * r;
}

// One layer near obstacles: central-difference curl of C(A psi_i), step 0.01 * swirl size (research R1).
glm::dvec3 TurbulenceSynthesis::constrainedLayer(glm::dvec3 x, const LayerConstants& lc) const {
    const double f = lc.frequency;
    const double width = s_.obstacleFade / f, h = 0.01 / f;
    auto potential = [&](glm::dvec3 p) {
        const glm::dvec3 psi = lc.potentialAmp * glm::dvec3(noise_.eval(f * p + lc.offset[0]).value,
                                                            noise_.eval(f * p + lc.offset[1]).value,
                                                            noise_.eval(f * p + lc.offset[2]).value);
        return constrainSlip(amplitudeAt(p) * psi, obstacles_->query(p), width);
    };
    const double s = 1.0 / (2.0 * h);
    const glm::dvec3 dx = (potential(x + glm::dvec3(h, 0, 0)) - potential(x - glm::dvec3(h, 0, 0))) * s;
    const glm::dvec3 dy = (potential(x + glm::dvec3(0, h, 0)) - potential(x - glm::dvec3(0, h, 0))) * s;
    const glm::dvec3 dz = (potential(x + glm::dvec3(0, 0, h)) - potential(x - glm::dvec3(0, 0, h))) * s;
    return {dy.z - dz.y, dz.x - dx.z, dx.y - dy.x};
}

}  // namespace ff::detail
