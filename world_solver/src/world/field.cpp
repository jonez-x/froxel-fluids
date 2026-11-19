#include "world/field.hpp"

#include <algorithm>
#include <cmath>

namespace ff::world {

namespace {

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

}  // namespace

Field::Field(std::size_t n) : n(n), data(n * n * n, 0.0f) {}

std::size_t Field::idx(std::size_t i, std::size_t j, std::size_t k) const {
    return i + n * (j + n * k);
}

float Field::get(std::size_t i, std::size_t j, std::size_t k) const {
    return data[idx(i, j, k)];
}

void Field::set(std::size_t i, std::size_t j, std::size_t k, float value) {
    std::size_t id = idx(i, j, k);
    data[id] = value;
}

float Field::sample_at_position(float x, float y, float z) const {
    // 1.001 so we never take the outer edges of the grid when using floor
    float cl_x = std::clamp(x, 0.0f, static_cast<float>(n) - 1.001f);
    float cl_y = std::clamp(y, 0.0f, static_cast<float>(n) - 1.001f);
    float cl_z = std::clamp(z, 0.0f, static_cast<float>(n) - 1.001f);

    std::size_t i0 = static_cast<std::size_t>(std::floor(cl_x));
    std::size_t j0 = static_cast<std::size_t>(std::floor(cl_y));
    std::size_t k0 = static_cast<std::size_t>(std::floor(cl_z));

    float d_x = cl_x - std::floor(cl_x);
    float d_y = cl_y - std::floor(cl_y);
    float d_z = cl_z - std::floor(cl_z);

    float c00 = lerp(get(i0, j0, k0), get(i0 + 1, j0, k0), d_x);
    float c10 = lerp(get(i0, j0 + 1, k0), get(i0 + 1, j0 + 1, k0), d_x);
    float c01 = lerp(get(i0, j0, k0 + 1), get(i0 + 1, j0, k0 + 1), d_x);
    float c11 = lerp(get(i0, j0 + 1, k0 + 1), get(i0 + 1, j0 + 1, k0 + 1), d_x);

    float c0 = lerp(c00, c10, d_y);
    float c1 = lerp(c01, c11, d_y);

    return lerp(c0, c1, d_z);
}

}  // namespace ff::world
