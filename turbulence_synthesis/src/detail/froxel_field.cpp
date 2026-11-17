#include "detail/froxel_field.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <optional>
#include <thread>

namespace ff::detail {

FroxelDetailField::FroxelDetailField(const camera::Camera& cam, const camera::FroxelGrid& grid)
    : cam_(cam), grid_(grid), data_(uint64_t(grid.nx) * grid.ny * grid.nz, glm::vec4(0.0f)) {}

FroxelDetailField::FroxelDetailField(const camera::Camera& cam, const camera::FroxelGrid& grid,
                                     std::vector<glm::vec4> data)
    : cam_(cam), grid_(grid), data_(std::move(data)) {
    data_.resize(uint64_t(grid.nx) * grid.ny * grid.nz, glm::vec4(0.0f));
}

glm::dvec3 FroxelDetailField::sample(glm::dvec3 x) const {
    const std::optional<glm::vec3> uvw = froxel_link::locate(cam_, grid_, glm::vec3(x));
    if (!uvw) return glm::dvec3(0.0);
    const glm::vec3 n(float(grid_.nx), float(grid_.ny), float(grid_.nz));
    const glm::vec3 g = glm::clamp(*uvw - 0.5f, glm::vec3(0.0f), n - 1.0f);
    const glm::ivec3 i0 = glm::min(glm::ivec3(g), glm::ivec3(n) - 1);
    const glm::ivec3 i1 = glm::min(i0 + 1, glm::ivec3(n) - 1);
    const glm::vec3 t = g - glm::vec3(i0);
    auto v = [&](int a, int b, int cc) { return glm::dvec3(glm::vec3(at(uint32_t(a), uint32_t(b), uint32_t(cc)))); };
    const glm::dvec3 c00 = glm::mix(v(i0.x, i0.y, i0.z), v(i1.x, i0.y, i0.z), double(t.x));
    const glm::dvec3 c10 = glm::mix(v(i0.x, i1.y, i0.z), v(i1.x, i1.y, i0.z), double(t.x));
    const glm::dvec3 c01 = glm::mix(v(i0.x, i0.y, i1.z), v(i1.x, i0.y, i1.z), double(t.x));
    const glm::dvec3 c11 = glm::mix(v(i0.x, i1.y, i1.z), v(i1.x, i1.y, i1.z), double(t.x));
    return glm::mix(glm::mix(c00, c10, double(t.y)), glm::mix(c01, c11, double(t.y)), double(t.z));
}

FroxelDetailField synthesizeFroxelsCpu(const TurbulenceSynthesis& synth, const camera::Camera& cam,
                                       const camera::FroxelGrid& grid) {
    FroxelDetailField field(cam, grid);
    std::vector<int> layers(grid.nz);
    for (uint32_t k = 0; k < grid.nz; ++k) layers[k] = synth.layersForSlice(cam, grid, k);

    std::atomic<uint32_t> nextSlice{0};
    auto worker = [&] {
        for (uint32_t k = nextSlice++; k < grid.nz; k = nextSlice++) {
            for (uint32_t j = 0; j < grid.ny; ++j)
                for (uint32_t i = 0; i < grid.nx; ++i) {
                    glm::vec4 out(0.0f, 0.0f, 0.0f, float(layers[k]));
                    if (layers[k] > 0) {
                        const glm::vec3 x = froxel_link::froxelCenter(cam, grid, i, j, k);
                        const glm::dvec3 u = synth.velocity(glm::dvec3(x), 1, layers[k]);
                        out = glm::vec4(glm::vec3(u), float(layers[k]));
                    }
                    field.data()[field.index(i, j, k)] = out;
                }
        }
    };
    const unsigned n = std::max(1u, std::min(grid.nz, std::thread::hardware_concurrency()));
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < n; ++t) pool.emplace_back(worker);
    for (auto& th : pool) th.join();
    return field;
}

}  // namespace ff::detail
