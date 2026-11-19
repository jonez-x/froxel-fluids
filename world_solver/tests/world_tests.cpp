#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "world/field.hpp"
#include "world/sim.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

using ff::world::Field;
using ff::world::Sim;

namespace {

float max_abs(const Field& f) {
    float m = 0.0f;
    for (float d : f.data) {
        m = std::max(m, std::abs(d));
    }
    return m;
}

}  // namespace

TEST_CASE("idx is x fastest") {
    Field f(4);
    CHECK(f.idx(1, 0, 0) == 1);
    CHECK(f.idx(0, 1, 0) == 4);
    CHECK(f.idx(0, 0, 1) == 16);
}

TEST_CASE("sampling at grid points returns the stored value") {
    Field f(4);
    f.set(1, 2, 1, 3.0f);
    CHECK(f.sample_at_position(1.0f, 2.0f, 1.0f) == doctest::Approx(3.0f));
}

TEST_CASE("sampling between two grid points interpolates linearly") {
    Field f(4);
    f.set(1, 1, 1, 2.0f);
    f.set(2, 1, 1, 4.0f);
    CHECK(f.sample_at_position(1.5f, 1.0f, 1.0f) == doctest::Approx(3.0f));
}

TEST_CASE("projection lowers the divergence") {
    Sim sim(16);
    sim.add_smoke_source();
    float before = max_abs(sim.divergence());
    sim.project();
    float after = max_abs(sim.divergence());
    REQUIRE(before > 0.0f);
    CHECK(after < before);
}

TEST_CASE("smoke rises") {
    Sim sim(24);
    for (int s = 0; s < 10; ++s) {
        sim.step(1.0f);
    }
    std::size_t c = sim.n / 2;
    CHECK(sim.r.get(c, 8, c) > 0.0f);
}
