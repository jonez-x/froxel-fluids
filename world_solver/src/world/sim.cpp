#include "world/sim.hpp"

#include <utility>

namespace ff::world {

namespace {

constexpr float BUOYANCY = 0.1f;
constexpr std::size_t JACOBI_ITERATIONS = 40;

}  // namespace

Sim::Sim(std::size_t n) : n(n), r(n), u(n), v(n), w(n) {}

void Sim::step(float dt) {
    add_smoke_source();
    add_buoyancy(dt);
    Field new_u = advect(u, dt);
    Field new_v = advect(v, dt);
    Field new_w = advect(w, dt);
    u = std::move(new_u);
    v = std::move(new_v);
    w = std::move(new_w);
    project();
    r = advect(r, dt);
}

void Sim::project() {
    Field div = divergence();
    Field p = pressure(div);
    for (std::size_t k = 1; k < n - 1; ++k) {
        for (std::size_t j = 1; j < n - 1; ++j) {
            for (std::size_t i = 1; i < n - 1; ++i) {
                float grad_x = 0.5f * (p.get(i + 1, j, k) - p.get(i - 1, j, k));
                float grad_y = 0.5f * (p.get(i, j + 1, k) - p.get(i, j - 1, k));
                float grad_z = 0.5f * (p.get(i, j, k + 1) - p.get(i, j, k - 1));
                u.set(i, j, k, u.get(i, j, k) - grad_x);
                v.set(i, j, k, v.get(i, j, k) - grad_y);
                w.set(i, j, k, w.get(i, j, k) - grad_z);
            }
        }
    }
}

void Sim::add_smoke_source() {
    // center coordinate of every direction of the grid
    std::size_t c = n / 2;
    // radius of the source basically half the sidelenght
    std::size_t rad = 4;
    for (std::size_t k = c - rad; k < c + rad; ++k) {
        for (std::size_t j = 1; j < 5; ++j) {
            for (std::size_t i = c - rad; i < c + rad; ++i) {
                r.set(i, j, k, 1.0f);
                v.set(i, j, k, 1.0f);
            }
        }
    }
}

Field Sim::advect(const Field& field, float dt) const {
    Field new_field(n);
    for (std::size_t k = 0; k < n; ++k) {
        for (std::size_t j = 0; j < n; ++j) {
            for (std::size_t i = 0; i < n; ++i) {
                float x_back = static_cast<float>(i) - dt * u.get(i, j, k);
                float y_back = static_cast<float>(j) - dt * v.get(i, j, k);
                float z_back = static_cast<float>(k) - dt * w.get(i, j, k);
                float new_sample = field.sample_at_position(x_back, y_back, z_back);
                new_field.set(i, j, k, new_sample);
            }
        }
    }
    return new_field;
}

Field Sim::divergence() const {
    Field new_field(n);
    for (std::size_t k = 1; k < n - 1; ++k) {
        for (std::size_t j = 1; j < n - 1; ++j) {
            for (std::size_t i = 1; i < n - 1; ++i) {
                float div = 0.5f * (u.get(i + 1, j, k) - u.get(i - 1, j, k)
                                    + v.get(i, j + 1, k) - v.get(i, j - 1, k)
                                    + w.get(i, j, k + 1) - w.get(i, j, k - 1));
                new_field.set(i, j, k, div);
            }
        }
    }
    return new_field;
}

Field Sim::pressure(const Field& div) const {
    Field p(n);
    Field p_next(n);
    for (std::size_t iteration = 0; iteration < JACOBI_ITERATIONS; ++iteration) {
        for (std::size_t k = 1; k < n - 1; ++k) {
            for (std::size_t j = 1; j < n - 1; ++j) {
                for (std::size_t i = 1; i < n - 1; ++i) {
                    float neighbors = p.get(i + 1, j, k) + p.get(i - 1, j, k)
                                      + p.get(i, j + 1, k) + p.get(i, j - 1, k)
                                      + p.get(i, j, k + 1) + p.get(i, j, k - 1);
                    p_next.set(i, j, k, (neighbors - div.get(i, j, k)) / 6.0f);
                }
            }
        }
        std::swap(p, p_next);
    }
    return p;
}

void Sim::add_buoyancy(float dt) {
    for (std::size_t k = 0; k < n; ++k) {
        for (std::size_t j = 0; j < n; ++j) {
            for (std::size_t i = 0; i < n; ++i) {
                v.set(i, j, k, v.get(i, j, k) + dt * BUOYANCY * r.get(i, j, k));
            }
        }
    }
}

}  // namespace ff::world
