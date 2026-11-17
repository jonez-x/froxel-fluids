// Turbulence synthesis (W5, spec 003): procedural sub-grid swirls per camera, on top of the world field.
//   u_detail(x) = sum_i curl( A(x) * psi_i(x) ),   i = 1 .. layers(x)
//   psi_i = 2^(-5/6 (i-1)) / f_i * noise(f_i x),  f_i = 2^i / h     (swirl size h / 2^i)
//   A(x)  = strength * h * |omega(x)| * ramp(y / d0)                (no swirls in still air, none at the ground)
// The curl is analytic, so the detail is divergence-free; stateless and deterministic.
// Obstacles (spec 009): within one swirl size of an obstacle, a layer is the finite-difference curl of
// the slip-constrained potential C(A psi_i) (Bridson 2007 eq. 5, signed ramp); farther away it is
// unchanged. Inside obstacles the detail is zero.
#pragma once

#include "froxel_link/froxel_link.hpp"
#include "detail/noise.hpp"
#include "detail/obstacles.hpp"
#include "detail/world_field.hpp"

#include <array>

namespace ff::detail {

enum class LayerRule {
    LargestEdge,  // layers from the largest froxel edge (spec 002 froxelSize)
    OnScreen,     // layers from the on-screen froxel size (ideas I27)
};

struct DetailSettings {
    LayerRule rule = LayerRule::LargestEdge;
    int maxLayers = 6;
    double strength = 1.0;
    uint32_t seed = 1;
    double wallDistance = -1.0;  // < 0: use the world cell size h
    double obstacleFade = 1.0;   // spec 009: influence zone of layer i = obstacleFade * swirl size (1 / f_i)
};

// Constants of one detail layer i (1-based). Single source for the CPU reference and the GPU shader
// (spec 004, research R2).
struct LayerConstants {
    double frequency = 0.0;     // f_i = 2^i / h  (swirl size h / 2^i)
    double amplitude = 0.0;     // a_i = 2^(-5/6 (i-1)), velocity falloff per layer
    double potentialAmp = 0.0;  // a_i / f_i
    glm::dvec3 offset[3]{};     // noise-space offset per potential channel (decorrelates channels and layers)
};
LayerConstants layerConstants(double cellSize, int layer);

struct DetailSample {
    glm::dvec3 velocity{0.0};
    int layers = 0;
};

class TurbulenceSynthesis {
public:
    // obstacles may be null (spec 003 behaviour); they must outlive the synthesis (spec 009).
    TurbulenceSynthesis(const WorldField& world, DetailSettings settings, const Obstacles* obstacles = nullptr);

    const DetailSettings& settings() const { return s_; }

    // Number of detail layers at x for this camera (0 outside the frustum).
    int layersFor(const camera::Camera& cam, const camera::FroxelGrid& grid, glm::dvec3 x) const;
    int layersForSlice(const camera::Camera& cam, const camera::FroxelGrid& grid, uint32_t k) const;

    // Detail velocity at x for this camera.
    DetailSample sample(const camera::Camera& cam, const camera::FroxelGrid& grid, glm::dvec3 x) const;

    // Detail velocity from layers [firstLayer, lastLayer] (1-based); zero at the ground and outside.
    glm::dvec3 velocity(glm::dvec3 x, int firstLayer, int lastLayer) const;

    const WorldField& world() const { return world_; }
    const GradientNoise& noise() const { return noise_; }
    double wallDistance() const { return d0_; }
    const Obstacles* obstacles() const { return obstacles_; }

private:
    glm::dvec3 constrainedLayer(glm::dvec3 x, const LayerConstants& lc) const;
    double amplitudeAt(glm::dvec3 x) const;

    const WorldField& world_;
    DetailSettings s_;
    GradientNoise noise_;
    double d0_;
    const Obstacles* obstacles_ = nullptr;
};

}  // namespace ff::detail
