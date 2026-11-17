// Static obstacles for the turbulence synthesis (spec 009, contracts/detail-obstacles-api.md).
// A small read-only interface so that the turbulence synthesis does not depend on a particular shape
// library; the host project implements it (e.g. curl::SolidsObstacles for the curl-noise shapes).
// Convention as in spec 005: distance > 0 in fluid, < 0 inside; normal = grad(distance), unit length.
#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace ff::detail {

struct ObstacleQuery {
    double distance = std::numeric_limits<double>::infinity();  // to the nearest obstacle
    glm::dvec3 normal{0.0};                                      // unit outward normal of the nearest obstacle
    double gap = std::numeric_limits<double>::infinity();        // second-nearest distance - distance
};

// GPU form of static obstacles, read by shaders/turbulence.wgsl (bindings 5 and 6).
// Layout of spec 006 (contracts/gpu-layout.md), 80 bytes per shape:
//   kind.x: 1 sphere, 2 box, 3 container, 4 half-space, 5 distance grid; kind.y: grid index
//   position.xyz: center, position.w: extent;  size.xyz: half size or normal, size.w: radius
//   motion.w: rounding (container);  angular: unused for static obstacles
// grids: per grid a 12-float header (lo.xyz, 1/spacing.xyz, dims.xyz, first sample index, 2 unused)
// followed by all headers' samples, x fastest. Empty vectors mean "no obstacles".
struct ObstacleShapeGpu {
    uint32_t kind[4] = {0, 0, 0, 0};
    float position[4] = {0, 0, 0, 0};
    float size[4] = {0, 0, 0, 0};
    float motion[4] = {0, 0, 0, 0};
    float angular[4] = {0, 0, 0, 0};
};
static_assert(sizeof(ObstacleShapeGpu) == 80);

struct ObstacleGpuData {
    std::vector<ObstacleShapeGpu> shapes;
    std::vector<float> grids;
};

class Obstacles {
public:
    virtual ~Obstacles() = default;
    virtual double distance(glm::dvec3 x) const = 0;
    virtual ObstacleQuery query(glm::dvec3 x) const = 0;

    // The same obstacles in the GPU layout above (needed by GpuTurbulence only). Throws
    // std::invalid_argument for obstacles the GPU cannot represent (moving or function-only shapes).
    virtual ObstacleGpuData gpuData() const {
        throw std::invalid_argument("these obstacles have no GPU form (implement Obstacles::gpuData)");
    }
};

}  // namespace ff::detail
