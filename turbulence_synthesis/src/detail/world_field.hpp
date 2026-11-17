// Coarse world field: velocity and density on an n^3 grid (spec 003, research R3/R8).
// Stand-in for the W1 world solver; read-only input of turbulence synthesis.
#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace ff::detail {

class WorldField {
public:
    WorldField(uint32_t n, double h, glm::dvec3 origin);

    uint32_t size() const { return n_; }
    double cellSize() const { return h_; }
    glm::dvec3 origin() const { return origin_; }
    bool inside(glm::dvec3 x) const;

    glm::dvec3& velocityAt(uint32_t i, uint32_t j, uint32_t k) { return vel_[index(i, j, k)]; }
    double& densityAt(uint32_t i, uint32_t j, uint32_t k) { return rho_[index(i, j, k)]; }
    glm::dvec3 cellCenter(uint32_t i, uint32_t j, uint32_t k) const;

    // Vorticity magnitude |curl u| per cell (central differences). Call after filling velocity.
    void computeVorticity();

    // Trilinear samples (cell-centered), zero outside the domain.
    glm::dvec3 sampleVelocity(glm::dvec3 x) const;
    double sampleDensity(glm::dvec3 x) const;

    // Vorticity magnitude with a quadratic B-spline (C1): value and exact gradient. Zero outside.
    struct Smooth {
        double value = 0.0;
        glm::dvec3 gradient{0.0};
    };
    Smooth sampleVorticitySmooth(glm::dvec3 x) const;

    // Per-cell vorticity magnitude, x fastest (uploaded by the GPU version, spec 004).
    const std::vector<double>& vorticityMagnitude() const { return vortMag_; }

private:
    uint64_t index(uint32_t i, uint32_t j, uint32_t k) const { return i + uint64_t(n_) * (j + uint64_t(n_) * k); }
    template <typename T>
    T trilinear(const std::vector<T>& f, glm::dvec3 x) const;

    uint32_t n_;
    double h_;
    glm::dvec3 origin_;
    std::vector<glm::dvec3> vel_;
    std::vector<double> rho_;
    std::vector<double> vortMag_;
};

// Synthetic scene: rising, swirling smoke plume above the ground plane y = 0 (research R8).
// 32^3 cells of 0.5 m, x and z in [-8, 8] m, y in [0, 16] m.
WorldField makePlumeScene();

// Solid-body rotation around +Y with angular velocity omega: |curl u| = 2 omega everywhere (for tests).
WorldField makeRotationScene(double omega = 1.0);

}  // namespace ff::detail
