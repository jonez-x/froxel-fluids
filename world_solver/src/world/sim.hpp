#pragma once

#include "world/field.hpp"

#include <cstddef>

namespace ff::world {

/// Sim struct that contains all data fields
struct Sim {
    /// grid size
    std::size_t n;
    /// density field
    Field r;
    /// velocity in x
    Field u;
    /// velocity in y
    Field v;
    /// velocity in z
    Field w;

    explicit Sim(std::size_t n);

    void step(float dt);
    void project();
    void add_smoke_source();
    Field advect(const Field& field, float dt) const;
    Field divergence() const;
    Field pressure(const Field& div) const;
    void add_buoyancy(float dt);
};

}  // namespace ff::world
