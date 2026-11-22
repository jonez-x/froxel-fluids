#include "transport/transport.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ff::transport {

namespace {

// Converts a 3D froxel index (i, j, k) into the index of the flat array.
std::size_t flatIndex(const camera::FroxelGrid& grid, uint32_t i, uint32_t j, uint32_t k) {
    return i + std::size_t(grid.nx) * (j + std::size_t(grid.ny) * k);
}

// Trilinear interpolation of values with a stencil from camera::historyStencil.
float trilinear(std::span<const float> values, const camera::FroxelGrid& grid,
                const camera::TrilinearStencil& stencil) {
    const glm::uvec3 low = stencil.lo;    
    const glm::uvec3 high = stencil.hi;  
    const glm::vec3 w = stencil.weights;  

   
    const float bottomFront = glm::mix(values[flatIndex(grid, low.x, low.y, low.z)],
                                       values[flatIndex(grid, high.x, low.y, low.z)], w.x);
    const float topFront = glm::mix(values[flatIndex(grid, low.x, high.y, low.z)],
                                    values[flatIndex(grid, high.x, high.y, low.z)], w.x);
    const float bottomBack = glm::mix(values[flatIndex(grid, low.x, low.y, high.z)],
                                      values[flatIndex(grid, high.x, low.y, high.z)], w.x);
    const float topBack = glm::mix(values[flatIndex(grid, low.x, high.y, high.z)],
                                   values[flatIndex(grid, high.x, high.y, high.z)], w.x);

    // Step 2: mix along y, the front and back face
    const float front = glm::mix(bottomFront, topFront, w.y);
    const float back = glm::mix(bottomBack, topBack, w.y);

    // Step 3: mix along z
    return glm::mix(front, back, w.z);
}

}  // namespace

// Calculates the lower bound of the detail at a froxel with world density rho
float boundLow(float rho, const Settings& s) {
    const float lo = rho * (1.0f - s.boundRelative) - s.boundAbsolute;
    return lo > 0.0f ? lo : 0.0f;
}

// Calculates the upper bound of the detail at a froxel with world density rho
float boundHigh(float rho, const Settings& s) {
    return rho * (1.0f + s.boundRelative) + s.boundAbsolute;
}

std::vector<float> transportStep(const camera::CameraHistory& history, const Settings& s,
                                 const FroxelFields& fields, std::span<const float> previous) {
    const camera::Camera& cam = history.current().camera;
    const camera::FroxelGrid& grid = history.current().grid;
    const std::size_t count = std::size_t(grid.nx) * grid.ny * grid.nz;

    std::vector<float> result(count, 0.0f);
    for (uint32_t k = 0; k < grid.nz; ++k) {
        for (uint32_t j = 0; j < grid.ny; ++j) {
            for (uint32_t i = 0; i < grid.nx; ++i) {
                const std::size_t index = flatIndex(grid, i, j, k);

                
                glm::vec3 position = grid.froxelCenter(cam, i, j, k);
                glm::vec3 velocity(fields.velocity[0][index], fields.velocity[1][index], fields.velocity[2][index]);
                float rho = fields.density[index];

                
                glm::vec3 backtraced = position - s.dt * velocity;
                camera::HistoryCoord old = camera::worldToPrevious(history, backtraced);

                
                float q = rho;
                if (old.sampleable) {
                    const camera::FroxelGrid& oldGrid = history.previous()->grid;
                    q = trilinear(previous, oldGrid, camera::historyStencil(oldGrid, old.uvw));
                }

                
                q = std::lerp(q, rho, s.relax);
                result[index] = std::clamp(q, boundLow(rho, s), boundHigh(rho, s));
            }
        }
    }
    return result;
}

}  // namespace ff::transport
