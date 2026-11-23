#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "camera/history.hpp"
#include "transport/transport.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace ff;

static float pattern(glm::vec3 p) { return 0.1f * p.x - 0.05f * p.y + 0.02f * p.z; }

TEST_CASE("camera moves, detail is interpolated from the old grid") {
    const camera::FroxelGrid grid{16, 9, 12};
    const std::size_t count = std::size_t(grid.nx) * grid.ny * grid.nz;

    camera::Camera oldCam;
    oldCam.zNear = 0.5f;
    oldCam.zFar = 10.0f;
    oldCam.aspect = 16.0f / 9.0f;

    camera::Camera newCam = oldCam;
    newCam.position.x = 0.2f;

    std::vector<float> previous;
    for (uint32_t k = 0; k < grid.nz; ++k)
        for (uint32_t j = 0; j < grid.ny; ++j)
            for (uint32_t i = 0; i < grid.nx; ++i)
                previous.push_back(pattern(grid.froxelCenter(oldCam, i, j, k)));

    camera::CameraHistory history;
    history.advance(oldCam, grid);
    history.advance(newCam, grid);

    const std::vector<float> zero(count, 0.0f);
    const std::vector<float> result = transport::transportStep(history, {}, {{zero, zero, zero}}, previous);

    int checked = 0;
    float maxError = 0.0f;
    std::size_t index = 0;
    for (uint32_t k = 0; k < grid.nz; ++k) {
        for (uint32_t j = 0; j < grid.ny; ++j) {
            for (uint32_t i = 0; i < grid.nx; ++i, ++index) {
                const glm::vec3 x = grid.froxelCenter(newCam, i, j, k);
                if (!camera::worldToPrevious(history, x).sampleable)
                    continue;
                maxError = std::max(maxError, std::abs(result[index] - pattern(x)));
                ++checked;
            }
        }
    }

    CHECK(checked > int(count) / 2);
    CHECK(maxError < 1e-3f);
}
