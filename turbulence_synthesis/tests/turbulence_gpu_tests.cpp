// GPU tests for turbulence synthesis (spec 004, T007): match with the CPU reference, exact zeros,
// determinism, odd grid sizes. Skipped (not failed) without a GPU.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "detail/froxel_field.hpp"
#include "detail/gpu_turbulence.hpp"
#include "gpu/context.hpp"

#include <cmath>
#include <cstring>
#include <optional>
#include <string>

using namespace ff::detail;
using ff::camera::Camera;
using ff::camera::FroxelGrid;
using ff::gpu::GpuContext;

namespace {

GpuContext* context() {
    static std::optional<GpuContext> ctx;
    static bool tried = false;
    if (!tried) {
        tried = true;
        try {
            ctx = GpuContext::create();
        } catch (const ff::gpu::NoAdapterError& e) {
            MESSAGE("skipped: ", e.what());
        }
    }
    return ctx ? &*ctx : nullptr;
}

Camera cameraA() {
    Camera c;
    c.position = {0.0f, 3.0f, 4.5f};
    return c;
}

Camera cameraB() {
    Camera c;
    c.position = {5.5f, 3.5f, 5.5f};
    c.yaw = -0.7853982f;  // looks towards the plume axis
    return c;
}

struct Comparison {
    double maxDiff = 0.0, maxRef = 0.0;
    int layerMismatch = 0, nonZeroWhereZero = 0, withDetail = 0;
};

Comparison compare(const FroxelDetailField& gpu, const FroxelDetailField& cpu) {
    Comparison c;
    for (size_t i = 0; i < cpu.data().size(); ++i) {
        const glm::vec4 g = gpu.data()[i], r = cpu.data()[i];
        c.maxRef = std::max(c.maxRef, double(glm::length(glm::vec3(r))));
        c.maxDiff = std::max(c.maxDiff, double(glm::length(glm::vec3(g) - glm::vec3(r))));
        c.layerMismatch += g.w != r.w;
        const bool cpuZero = r.x == 0.0f && r.y == 0.0f && r.z == 0.0f;
        const bool gpuZero = g.x == 0.0f && g.y == 0.0f && g.z == 0.0f;
        c.nonZeroWhereZero += cpuZero && !gpuZero;
        c.withDetail += !cpuZero;
    }
    return c;
}

}  // namespace

TEST_CASE("GPU detail matches the CPU reference for both cameras and both rules (SC-001, SC-003)") {
    GpuContext* ctx = context();
    if (!ctx) return;
    const WorldField world = makePlumeScene();
    const FroxelGrid grid;
    for (LayerRule rule : {LayerRule::LargestEdge, LayerRule::OnScreen}) {
        DetailSettings s;
        s.rule = rule;
        GpuTurbulence gt(*ctx, world, s);
        FroxelDetail fd(*ctx, grid);
        for (const Camera& cam : {cameraA(), cameraB()}) {
            fd.synthesize(gt, cam);
            const FroxelDetailField gpu = fd.readBack(cam);
            const FroxelDetailField cpu = synthesizeFroxelsCpu(gt.reference(), cam, grid);
            const Comparison c = compare(gpu, cpu);
            MESSAGE(std::string(rule == LayerRule::LargestEdge ? "largest-edge" : "on-screen"), " camera (", cam.position.x, ", ",
                    cam.position.y, ", ", cam.position.z, "): max diff / max ref = ", c.maxDiff / c.maxRef,
                    ", froxels with detail ", c.withDetail, ", layer mismatches ", c.layerMismatch);
            CHECK(c.withDetail > 0);
            CHECK(c.maxDiff <= 1e-3 * c.maxRef);
            CHECK(c.layerMismatch == 0);
            CHECK(c.nonZeroWhereZero == 0);  // exact zeros where the reference is zero
        }
    }
}

TEST_CASE("GPU synthesis is deterministic (SC-003)") {
    GpuContext* ctx = context();
    if (!ctx) return;
    const WorldField world = makePlumeScene();
    const FroxelGrid grid;
    GpuTurbulence gt(*ctx, world, {});
    FroxelDetail fd(*ctx, grid);
    fd.synthesize(gt, cameraA());
    const FroxelDetailField a = fd.readBack(cameraA());
    fd.synthesize(gt, cameraB());  // something else in between
    fd.synthesize(gt, cameraA());
    const FroxelDetailField b = fd.readBack(cameraA());
    CHECK(std::memcmp(a.data().data(), b.data().data(), a.data().size() * sizeof(glm::vec4)) == 0);
}

TEST_CASE("odd grid size: every froxel is written exactly once") {
    GpuContext* ctx = context();
    if (!ctx) return;
    const WorldField world = makePlumeScene();
    FroxelGrid grid;
    grid.nx = 37;
    grid.ny = 23;
    grid.nz = 19;
    GpuTurbulence gt(*ctx, world, {});
    FroxelDetail fd(*ctx, grid);
    fd.synthesize(gt, cameraA());
    const FroxelDetailField gpu = fd.readBack(cameraA());
    const FroxelDetailField cpu = synthesizeFroxelsCpu(gt.reference(), cameraA(), grid);
    const Comparison c = compare(gpu, cpu);
    MESSAGE("37x23x19: max diff / max ref = ", c.maxDiff / c.maxRef, ", froxels with detail ", c.withDetail);
    CHECK(c.withDetail > 0);
    CHECK(c.maxDiff <= 1e-3 * c.maxRef);
    CHECK(c.layerMismatch == 0);
}

