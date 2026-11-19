#pragma once

#include <cstddef>
#include <vector>

namespace ff::world {

struct Field {
    std::size_t n;
    std::vector<float> data;

    explicit Field(std::size_t n);

    std::size_t idx(std::size_t i, std::size_t j, std::size_t k) const;
    float get(std::size_t i, std::size_t j, std::size_t k) const;
    void set(std::size_t i, std::size_t j, std::size_t k, float value);
    float sample_at_position(float x, float y, float z) const;
};

}  // namespace ff::world
