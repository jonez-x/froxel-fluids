#include "detail/preview.hpp"

#include "util/png.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <thread>

namespace ff::detail {

PreviewImage renderPreview(const WorldField& world, const PreviewDetail* detail, const camera::Camera& cam,
                           const camera::FroxelGrid& grid, const PreviewOptions& o) {
    const size_t npx = size_t(o.width) * o.height;
    PreviewImage img{o.width, o.height, std::vector<float>(npx), std::vector<int>(npx), std::vector<uint8_t>(npx)};
    const glm::mat3 rot(cam.invView());
    const float t = std::tan(cam.fovY * 0.5f);
    const int steps = 2 * int(grid.nz);
    const double sigma = 0.9, maxDepth = 30.0;

    std::atomic<int> nextRow{0};
    auto worker = [&] {
        for (int py = nextRow++; py < o.height; py = nextRow++) {
            for (int px = 0; px < o.width; ++px) {
                const float ndcX = (px + 0.5f) / o.width * 2.0f - 1.0f;
                const float ndcY = 1.0f - (py + 0.5f) / o.height * 2.0f;
                const glm::dvec3 dirView(ndcX * t * cam.aspect, ndcY * t, -1.0);  // view depth 1
                const glm::dvec3 dir = glm::dvec3(rot * glm::vec3(dirView));
                const double dirLen = glm::length(dirView);
                double T = 1.0, L = 0.0;
                int maxL = 0;
                bool touched = false;
                for (int s = 0; s < steps && T > 0.005; ++s) {
                    const double d0 = froxel_link::sliceDepth(cam, grid, s * 0.5f), d1 = froxel_link::sliceDepth(cam, grid, (s + 1) * 0.5f);
                    if (d0 > maxDepth) break;
                    const double dm = 0.5 * (d0 + d1);
                    const glm::dvec3 x = glm::dvec3(cam.position) + dm * dir;
                    double rho = world.sampleDensity(x);
                    double layersHere = 0;
                    if (detail && rho > 1e-4) {
                        const int layers = detail->layersOfSlice[std::min<uint32_t>(grid.nz - 1, uint32_t(s / 2))];
                        if (layers > 0) {
                            layersHere = layers;
                            touched = true;
                            const glm::dvec3 u = detail->velocity(x, layers);
                            rho = world.sampleDensity(x - o.displace * u);
                        }
                    }
                    const double a = 1.0 - std::exp(-sigma * rho * (d1 - d0) * dirLen);
                    const double light = 0.75 + 0.25 * std::clamp(x.y / 8.0, 0.0, 1.0);
                    L += T * a * light;
                    if (layersHere > 0 && T * a > 0.01) maxL = std::max(maxL, int(layersHere));  // detail visibly contributes
                    T *= 1.0 - a;
                }
                const double bg = 0.10 + 0.08 * (1.0 - (py + 0.5) / o.height);
                img.v[size_t(py) * o.width + px] = float(std::clamp(L + T * bg, 0.0, 1.0));
                img.maxLayers[size_t(py) * o.width + px] = maxL;
                img.touched[size_t(py) * o.width + px] = touched;
            }
        }
    };
    const unsigned n = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> pool;
    for (unsigned i = 0; i < n; ++i) pool.emplace_back(worker);
    for (auto& th : pool) th.join();
    return img;
}

bool savePreview(const PreviewImage& img, const std::string& path) {
    std::vector<uint8_t> px(img.v.size());
    for (size_t i = 0; i < px.size(); ++i) px[i] = uint8_t(std::lround(255.0 * std::pow(img.v[i], 1.0 / 2.2)));
    return png::writeGray(path, img.w, img.h, px);
}

double hfEnergy(const PreviewImage& img, const std::function<bool(size_t)>& mask) {
    double e = 0.0;
    for (int y = 1; y + 1 < img.h; ++y)
        for (int x = 1; x + 1 < img.w; ++x) {
            const size_t i = size_t(y) * img.w + x;
            if (!mask(i)) continue;
            const double lap = 4 * img.v[i] - img.v[i - 1] - img.v[i + 1] - img.v[i - img.w] - img.v[i + img.w];
            e += lap * lap;
        }
    return e;
}

}  // namespace ff::detail
