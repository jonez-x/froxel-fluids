// turbulence_demo (spec 003): renders the synthetic plume from one camera three times:
// world grid only, world + detail (largest-edge rule), world + detail (on-screen rule),
// and prints the metrics of contracts/turbulence-demo-cli.md.
#include "camera/froxel_grid.hpp"
#include "detail/turbulence.hpp"
#include "detail/world_field.hpp"
#include "detail/preview.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace ff;

namespace {

struct Options {
    std::string out = ".";
    double strength = 1.0;
    double displace = 1.0;  // tau (s): density is displaced by tau * u_detail
    int width = 480, height = 270;
};

camera::Camera demoCamera() {
    camera::Camera c;
    c.position = {0.0f, 3.0f, 4.5f};  // 4.5 m in front of the plume axis
    c.yaw = 0.0f;
    c.pitch = 0.0f;
    return c;
}

// Detail evaluated exactly at each sample point (spec 003), for the shared preview renderer.
detail::PreviewDetail exactDetail(const detail::TurbulenceSynthesis& t, const camera::Camera& cam,
                                  const camera::FroxelGrid& grid) {
    detail::PreviewDetail d;
    d.layersOfSlice.resize(grid.nz);
    for (uint32_t k = 0; k < grid.nz; ++k) d.layersOfSlice[k] = t.layersForSlice(cam, grid, k);
    d.velocity = [&t](glm::dvec3 x, int layers) { return t.velocity(x, 1, layers); };
    return d;
}

struct Reach {
    double depth = 0.0;
    int slices = 0;
};
Reach reachOf(const detail::TurbulenceSynthesis& t, const camera::Camera& cam, const camera::FroxelGrid& g) {
    Reach r;
    for (uint32_t k = 0; k < g.nz; ++k)
        if (t.layersForSlice(cam, g, k) > 0) {
            ++r.slices;
            r.depth = g.sliceDepth(cam, float(k + 1));
        }
    return r;
}

}  // namespace

int main(int argc, char** argv) {
    Options o;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string_view k = argv[i];
        if (k == "--out") o.out = argv[i + 1];
        else if (k == "--strength") o.strength = std::atof(argv[i + 1]);
        else if (k == "--displace") o.displace = std::atof(argv[i + 1]);
        else if (k == "--width") o.width = std::atoi(argv[i + 1]);
        else if (k == "--height") o.height = std::atoi(argv[i + 1]);
        else {
            std::fprintf(stderr, "usage: turbulence_demo [--out DIR] [--strength S] [--displace T] [--width W] [--height H]\n");
            return 3;
        }
    }
    if ((argc - 1) % 2 != 0 || o.width < 8 || o.height < 8) {
        std::fprintf(stderr, "usage: turbulence_demo [--out DIR] [--strength S] [--displace T] [--width W] [--height H]\n");
        return 3;
    }
    const auto t0 = std::chrono::steady_clock::now();
    const detail::WorldField world = detail::makePlumeScene();
    const camera::Camera cam = demoCamera();
    camera::FroxelGrid grid;
    camera::Camera c = cam;
    c.aspect = float(o.width) / float(o.height);

    detail::DetailSettings sl, so;
    sl.strength = so.strength = o.strength;
    sl.rule = detail::LayerRule::LargestEdge;
    so.rule = detail::LayerRule::OnScreen;
    const detail::TurbulenceSynthesis largest(world, sl), onscreen(world, so);

    const detail::PreviewOptions po{o.width, o.height, o.displace};
    const detail::PreviewDetail dl = exactDetail(largest, c, grid), dn = exactDetail(onscreen, c, grid);
    const detail::PreviewImage imgWorld = detail::renderPreview(world, nullptr, c, grid, po);
    const detail::PreviewImage imgLargest = detail::renderPreview(world, &dl, c, grid, po);
    const detail::PreviewImage imgOn = detail::renderPreview(world, &dn, c, grid, po);

    std::filesystem::create_directories(o.out);
    const std::string base = o.out + "/";
    if (!detail::savePreview(imgWorld, base + "turbulence_world.png") ||
        !detail::savePreview(imgLargest, base + "turbulence_detail_largest.png") ||
        !detail::savePreview(imgOn, base + "turbulence_detail_onscreen.png")) {
        std::fprintf(stderr, "error: could not write images to %s\n", o.out.c_str());
        return 1;
    }

    // Metrics (SC-004, SC-006).
    auto withDetail = [](const detail::PreviewImage& d) {
        return std::function<bool(size_t)>([&d](size_t i) { return d.maxLayers[i] > 0; });
    };
    const double hwL = detail::hfEnergy(imgWorld, withDetail(imgLargest)), hdL = detail::hfEnergy(imgLargest, withDetail(imgLargest));
    const double hwO = detail::hfEnergy(imgWorld, withDetail(imgOn)), hdO = detail::hfEnergy(imgOn, withDetail(imgOn));
    double maxDiff = 0.0;
    for (size_t i = 0; i < imgWorld.v.size(); ++i) {
        if (!imgLargest.touched[i]) maxDiff = std::max(maxDiff, double(std::abs(imgLargest.v[i] - imgWorld.v[i])));
        if (!imgOn.touched[i]) maxDiff = std::max(maxDiff, double(std::abs(imgOn.v[i] - imgWorld.v[i])));
    }
    size_t pxL = 0, pxO = 0;
    for (size_t i = 0; i < imgWorld.v.size(); ++i) {
        pxL += imgLargest.maxLayers[i] > 0;
        pxO += imgOn.maxLayers[i] > 0;
    }
    const Reach rl = reachOf(largest, c, grid), ro = reachOf(onscreen, c, grid);
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto pct = [](double a, double b) { return b > 0 ? 100.0 * (a / b - 1.0) : 0.0; };

    char buf[2048];
    std::snprintf(buf, sizeof buf,
                  "scene: plume %u^3, cell %.2f m; camera (%.1f, %.1f, %.1f); grid %ux%ux%u; image %dx%d\n"
                  "detail reach: largest-edge up to %.2f m (%d/%u slices), on-screen up to %.2f m (%d/%u slices)\n"
                  "pixels with detail: largest-edge %.1f %%, on-screen %.1f %%\n"
                  "hf energy where detail applies: largest-edge world %.4g, detail %.4g (%+.0f %%); "
                  "on-screen world %.4g, detail %.4g (%+.0f %%)\n"
                  "max difference where no detail layer was used: %.3f %%\n"
                  "strength %.2f, displace %.2f s\n"
                  "time: %.1f s\n",
                  world.size(), world.cellSize(), c.position.x, c.position.y, c.position.z, grid.nx, grid.ny, grid.nz,
                  o.width, o.height, rl.depth, rl.slices, grid.nz, ro.depth, ro.slices, grid.nz,
                  100.0 * pxL / imgWorld.v.size(), 100.0 * pxO / imgWorld.v.size(), hwL, hdL, pct(hdL, hwL), hwO, hdO,
                  pct(hdO, hwO), 100.0 * maxDiff, o.strength, o.displace, secs);
    std::fputs(buf, stdout);
    if (FILE* f = std::fopen((base + "turbulence_summary.txt").c_str(), "w")) {
        std::fputs(buf, f);
        std::fclose(f);
    }
    return 0;
}
