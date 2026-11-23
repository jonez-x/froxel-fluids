#include "transport/transport.hpp"

#include <glm/glm.hpp>

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

    // Mix along y, the front and back face
    const float front = glm::mix(bottomFront, topFront, w.y);
    const float back = glm::mix(bottomBack, topBack, w.y);

    // Mix along z
    return glm::mix(front, back, w.z);
}

}  // namespace

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

                glm::vec3 backtraced = position - s.dt * velocity;
                camera::HistoryCoord old = camera::worldToPrevious(history, backtraced);

                // seen before: carry the old detail over, otherwise newly visible
                float d = 0.0f;
                if (old.sampleable) {
                    const camera::FroxelGrid& oldGrid = history.previous()->grid;
                    d = trilinear(previous, oldGrid, camera::historyStencil(oldGrid, old.uvw)) * s.decay;
                }

                result[index] = d;
            }
        }
    }
    return result;
}

}
