// Tests for turbulence synthesis (spec 003, US2 + US3): divergence-free, Kolmogorov falloff,
// zero where it must be zero, deterministic, layer rules.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "detail/turbulence.hpp"

#include <cmath>
#include <random>

using namespace ff::detail;
using ff::camera::Camera;
using ff::camera::FroxelGrid;

namespace {

Camera demoCamera() {
    Camera c;
    c.position = {0.0f, 3.0f, 4.5f};
    c.yaw = 0.0f;  // looks along -Z, towards the plume axis
    c.pitch = 0.0f;
    return c;
}

// Jacobian of u at x by central differences (step e).
glm::dmat3 jacobian(const TurbulenceSynthesis& t, glm::dvec3 x, int a, int b, double e) {
    glm::dmat3 J(0.0);
    for (int d = 0; d < 3; ++d) {
        glm::dvec3 dx(0.0);
        dx[d] = e;
        J[d] = (t.velocity(x + dx, a, b) - t.velocity(x - dx, a, b)) / (2.0 * e);  // column d = du/dx_d
    }
    return J;
}

}  // namespace

TEST_CASE("noise gradient matches finite differences") {
    GradientNoise n(7);
    std::mt19937 rng(3);
    std::uniform_real_distribution<double> d(-20.0, 20.0);
    const double e = 1e-6;
    for (int s = 0; s < 200; ++s) {
        const glm::dvec3 p(d(rng), d(rng), d(rng));
        const auto g = n.eval(p).gradient;
        for (int a = 0; a < 3; ++a) {
            glm::dvec3 dp(0.0);
            dp[a] = e;
            const double fd = (n.eval(p + dp).value - n.eval(p - dp).value) / (2 * e);
            CHECK(g[a] == doctest::Approx(fd).epsilon(1e-5).scale(1.0));
        }
    }
}

TEST_CASE("detail velocity is divergence-free (SC-001)") {
    const WorldField w = makePlumeScene();
    TurbulenceSynthesis t(w, {});
    std::mt19937 rng(11);
    std::uniform_real_distribution<double> rx(-2.5, 2.5), ry(1.0, 8.0);
    double worst = 0.0;
    int nonzero = 0;
    for (int s = 0; s < 1000; ++s) {
        const glm::dvec3 x(rx(rng), ry(rng), rx(rng));
        const glm::dmat3 J = jacobian(t, x, 1, 3, 1e-5);
        const double div = J[0][0] + J[1][1] + J[2][2];
        double norm = 0.0;
        for (int c = 0; c < 3; ++c)
            for (int r = 0; r < 3; ++r) norm += J[c][r] * J[c][r];
        norm = std::sqrt(norm);
        if (norm < 1e-9) continue;  // still air
        ++nonzero;
        worst = std::max(worst, std::abs(div) / norm);
    }
    MESSAGE("worst relative divergence: ", worst, " over ", nonzero, " points");
    CHECK(nonzero > 500);
    CHECK(worst < 0.01);
}

TEST_CASE("layer energy falls by 2^(-5/3) per layer (SC-002)") {
    const WorldField w = makeRotationScene(1.0);  // constant |omega| = 2: constant amplitude
    TurbulenceSynthesis t(w, {});
    std::mt19937 rng(5);
    std::uniform_real_distribution<double> rx(-5.0, 5.0), ry(2.0, 12.0);
    double e[5] = {0, 0, 0, 0, 0};
    const int N = 4000;
    for (int s = 0; s < N; ++s) {
        const glm::dvec3 x(rx(rng), ry(rng), rx(rng));
        for (int i = 1; i <= 4; ++i) {
            const glm::dvec3 u = t.velocity(x, i, i);
            e[i] += glm::dot(u, u) / N;
        }
    }
    const double expected = std::pow(2.0, -5.0 / 3.0);
    for (int i = 1; i < 4; ++i) {
        const double ratio = e[i + 1] / e[i];
        MESSAGE("energy ratio layer ", i + 1, "/", i, ": ", ratio, " (expected ", expected, ")");
        CHECK(ratio == doctest::Approx(expected).epsilon(0.25));
    }
}

TEST_CASE("detail is exactly zero where it must be (SC-003)") {
    const WorldField w = makePlumeScene();
    TurbulenceSynthesis t(w, {});
    const Camera cam = demoCamera();
    const FroxelGrid g;
    CHECK(t.velocity({0.3, 0.0, 0.2}, 1, 4) == glm::dvec3(0.0));   // on the ground
    CHECK(t.velocity({0.3, -1.0, 0.2}, 1, 4) == glm::dvec3(0.0));  // below the ground
    CHECK(t.velocity({50.0, 3.0, 0.0}, 1, 4) == glm::dvec3(0.0));  // outside the world domain
    // just above the ground the detail is much weaker than one cell higher (smooth fade)
    const double low = glm::length(t.velocity({0.5, 0.01, 0.5}, 1, 3));
    const double high = glm::length(t.velocity({0.5, 1.5, 0.5}, 1, 3));
    CHECK(low < 0.05 * high);
    // a far point (froxel larger than a world cell) gets 0 layers and exactly zero detail
    const glm::dvec3 farPoint(0.0, 3.0, 4.5 - 60.0);
    const DetailSample s = t.sample(cam, g, farPoint);
    CHECK(s.layers == 0);
    CHECK(s.velocity == glm::dvec3(0.0));
}

TEST_CASE("deterministic: same inputs give the same output (FR-007)") {
    const WorldField w = makePlumeScene();
    TurbulenceSynthesis a(w, {}), b(w, {});
    const glm::dvec3 x(0.7, 3.3, -0.4);
    CHECK(a.velocity(x, 1, 5) == b.velocity(x, 1, 5));
    DetailSettings other;
    other.seed = 2;
    TurbulenceSynthesis c(w, other);
    CHECK(c.velocity(x, 1, 5) != a.velocity(x, 1, 5));
}

TEST_CASE("on-screen rule never gives fewer layers than largest-edge (US3)") {
    const WorldField w = makePlumeScene();
    DetailSettings on;
    on.rule = LayerRule::OnScreen;
    TurbulenceSynthesis largest(w, {}), onscreen(w, on);
    const Camera cam = demoCamera();
    const FroxelGrid g;
    int moreSomewhere = 0;
    for (uint32_t k = 0; k < g.nz; ++k) {
        const int a = largest.layersForSlice(cam, g, k), b = onscreen.layersForSlice(cam, g, k);
        CHECK(b >= a);
        CHECK(a <= largest.settings().maxLayers);
        if (b > a) ++moreSomewhere;
    }
    CHECK(moreSomewhere > 0);
}

// ------------------------------------------------------------------ spec 004: CPU froxel detail field

#include "detail/froxel_field.hpp"

TEST_CASE("froxel detail field: CPU fill has exact zeros where no layers apply; sample() hits stored values") {
    const WorldField w = makePlumeScene();
    TurbulenceSynthesis t(w, {});
    const Camera cam = demoCamera();
    FroxelGrid grid;
    grid.nx = 40;
    grid.ny = 24;
    grid.nz = 32;
    const FroxelDetailField f = synthesizeFroxelsCpu(t, cam, grid);
    int zeroLayers = 0, withDetail = 0;
    for (uint32_t k = 0; k < grid.nz; ++k) {
        const int layers = t.layersForSlice(cam, grid, k);
        for (uint32_t j = 0; j < grid.ny; ++j)
            for (uint32_t i = 0; i < grid.nx; ++i) {
                const glm::vec4 v = f.at(i, j, k);
                CHECK(v.w == float(layers));
                if (layers == 0) {
                    ++zeroLayers;
                    CHECK(glm::vec3(v) == glm::vec3(0.0f));
                } else if (glm::vec3(v) != glm::vec3(0.0f)) {
                    ++withDetail;
                }
            }
    }
    CHECK(zeroLayers > 0);
    CHECK(withDetail > 0);
    // at a froxel centre, the trilinear sample returns the stored value
    for (uint32_t k : {2u, 5u, 9u}) {
        const glm::vec3 x = grid.froxelCenter(cam, 17, 11, k);
        const glm::dvec3 s = f.sample(glm::dvec3(x));
        const glm::vec3 stored(f.at(17, 11, k));
        CHECK(glm::length(glm::vec3(s) - stored) <= 1e-3f * (glm::length(stored) + 1e-6f));
    }
}

