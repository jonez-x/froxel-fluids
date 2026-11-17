// CPU preview ray-marcher for detail images (spec 003, moved here by spec 004, research R8).
// Renders the world density along camera rays through exponential slices; where detail applies,
// the density is displaced by tau * u_detail. The detail comes from a callback, so the same code
// renders exactly evaluated detail (spec 003) and detail read from a froxel buffer (spec 004).
#pragma once

#include "froxel_link/froxel_link.hpp"
#include "detail/world_field.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ff::detail {

struct PreviewOptions {
    int width = 480, height = 270;
    double displace = 1.0;  // tau (s): density is displaced by tau * u_detail
};

struct PreviewImage {
    int w = 0, h = 0;
    std::vector<float> v;          // radiance in [0, 1]
    std::vector<int> maxLayers;    // largest layer count of samples that visibly contribute (T*alpha > 1 %)
    std::vector<uint8_t> touched;  // 1 if any sample along the ray used detail at all
};

// Where the detail comes from: the layer count per slice (0 = no detail) and the velocity at x.
struct PreviewDetail {
    std::vector<int> layersOfSlice;
    std::function<glm::dvec3(glm::dvec3 x, int layers)> velocity;
};

// Ray-marches one image. detail == nullptr renders the world grid only. Multithreaded, deterministic.
PreviewImage renderPreview(const WorldField& world, const PreviewDetail* detail, const camera::Camera& cam,
                           const camera::FroxelGrid& grid, const PreviewOptions& o);

// 8-bit gray PNG with gamma 1/2.2. Returns false on write error.
bool savePreview(const PreviewImage& img, const std::string& path);

// Sum of squared Laplacians over pixels where mask(i) is true (high-frequency energy).
double hfEnergy(const PreviewImage& img, const std::function<bool(size_t)>& mask);

}  // namespace ff::detail
