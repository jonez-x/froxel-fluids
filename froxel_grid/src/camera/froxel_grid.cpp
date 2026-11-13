#include "camera/froxel_grid.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace ff::camera {
namespace {

void validateDimension(uint32_t n) {
    if (n == 0 || n > (1u << 23)) {
        throw std::invalid_argument("froxel dimensions must be in [1, 2^23] for exact float cell-center coordinates");
    }
}

bool finite(glm::vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }

float sliceDepthUnchecked(const Camera& cam, uint32_t nz, float k) {
    if (k == 0.0f) return cam.zNear;
    if (k == float(nz)) return cam.zFar;
    return cam.zNear * std::pow(cam.zFar / cam.zNear, k / float(nz));
}

float depthToSliceUnchecked(const Camera& cam, uint32_t nz, float d) {
    if (d == cam.zNear) return 0.0f;
    if (d == cam.zFar) return float(nz);
    return float(nz) * std::log(d / cam.zNear) / std::log(cam.zFar / cam.zNear);
}

}  // namespace

void FroxelGrid::validate() const {
    validateDimension(nx);
    validateDimension(ny);
    validateDimension(nz);
}

float FroxelGrid::sliceDepth(const Camera& cam, uint32_t nz, float k) {
    cam.validate();
    validateDimension(nz);
    if (!std::isfinite(k)) throw std::invalid_argument("slice coordinate must be finite");
    const float d = sliceDepthUnchecked(cam, nz, k);
    if (!std::isfinite(d) || d <= 0.0f) throw std::overflow_error("slice depth is not representable as positive finite float");
    return d;
}

float FroxelGrid::depthToSlice(const Camera& cam, uint32_t nz, float d) {
    cam.validate();
    validateDimension(nz);
    if (!std::isfinite(d) || d <= 0.0f) throw std::invalid_argument("view depth must be positive and finite");
    const float w = depthToSliceUnchecked(cam, nz, d);
    if (!std::isfinite(w)) throw std::overflow_error("slice coordinate is not representable as finite float");
    return w;
}

FroxelCoord FroxelGrid::worldToFroxel(const Camera& cam, glm::vec3 world) const {
    validate();
    cam.validate();
    FroxelCoord c;
    if (!finite(world)) return c;
    const glm::vec4 p = cam.view() * glm::vec4(world, 1.0f);
    const float d = -p.z;  // depth along the view direction
    if (!finite(glm::vec3(p)) || d <= 0.0f) return c;  // behind/at the camera or unrepresentable

    const float t = std::tan(cam.fovY * 0.5f);
    const float halfHeight = d * t, halfWidth = halfHeight * cam.aspect;
    if (!std::isfinite(halfHeight) || !std::isfinite(halfWidth) || halfHeight <= 0.0f || halfWidth <= 0.0f) return c;
    const float ndcX = p.x / halfWidth;
    const float ndcY = p.y / halfHeight;
    const glm::vec3 uvw((ndcX * 0.5f + 0.5f) * float(nx), (ndcY * 0.5f + 0.5f) * float(ny),
                        depthToSliceUnchecked(cam, nz, d));
    if (!finite(uvw)) return c;
    c.uvw = uvw;
    // This is an index contract, not a closed geometric frustum. In particular, far,
    // right and top boundaries must never floor to the out-of-range index n.
    c.inside = uvw.x >= 0.0f && uvw.x < float(nx) && uvw.y >= 0.0f && uvw.y < float(ny) &&
               uvw.z >= 0.0f && uvw.z < float(nz);
    return c;
}

glm::vec3 FroxelGrid::froxelToWorld(const Camera& cam, glm::vec3 uvw) const {
    validate();
    cam.validate();
    if (!finite(uvw)) throw std::invalid_argument("froxel coordinates must be finite");
    const float d = sliceDepthUnchecked(cam, nz, uvw.z);
    if (!std::isfinite(d) || d <= 0.0f) throw std::overflow_error("inverse mapping depth is not representable as positive finite float");
    const float t = std::tan(cam.fovY * 0.5f);
    const float ndcX = uvw.x / float(nx) * 2.0f - 1.0f;
    const float ndcY = uvw.y / float(ny) * 2.0f - 1.0f;
    const glm::vec4 p(ndcX * t * cam.aspect * d, ndcY * t * d, -d, 1.0f);
    const glm::vec3 world(cam.invView() * p);
    if (!finite(world)) throw std::overflow_error("inverse mapping result is not representable as finite float");
    return world;
}

glm::vec3 FroxelGrid::froxelCenter(const Camera& cam, uint32_t i, uint32_t j, uint32_t k) const {
    validate();
    if (i >= nx || j >= ny || k >= nz) throw std::out_of_range("froxel center index outside grid");
    return froxelToWorld(cam, glm::vec3(float(i) + 0.5f, float(j) + 0.5f, float(k) + 0.5f));
}

float FroxelGrid::froxelSize(const Camera& cam, uint32_t k) const {
    validate();
    if (k >= nz) throw std::out_of_range("froxel size slice outside grid");
    const float d0 = sliceDepth(cam, float(k));
    const float d1 = sliceDepth(cam, float(k + 1));
    const float dMid = sliceDepth(cam, float(k) + 0.5f);
    const float height = 2.0f * std::tan(cam.fovY * 0.5f) * dMid / float(ny);
    const float width = 2.0f * std::tan(cam.fovY * 0.5f) * cam.aspect * dMid / float(nx);
    return std::max({width, height, d1 - d0});
}

float FroxelGrid::froxelScreenSize(const Camera& cam, uint32_t k) const {
    validate();
    if (k >= nz) throw std::out_of_range("froxel size slice outside grid");
    const float dMid = sliceDepth(cam, float(k) + 0.5f);
    const float height = 2.0f * std::tan(cam.fovY * 0.5f) * dMid / float(ny);
    const float width = 2.0f * std::tan(cam.fovY * 0.5f) * cam.aspect * dMid / float(nx);
    return std::max(width, height);
}

int detailLayers(float froxelSize, float worldCell) {
    if (!std::isfinite(froxelSize) || !std::isfinite(worldCell))
        throw std::invalid_argument("detail layer sizes must be finite");
    if (froxelSize <= 0.0f || worldCell <= 0.0f) return 0;
    // Do not form a float ratio: valid finite sizes can overflow or underflow it.
    const double octaves = std::log2(double(worldCell)) - std::log2(double(froxelSize));
    return std::max(0, int(std::lround(octaves)));
}

FroxelUniform makeUniform(const Camera& cam, const FroxelGrid& grid) {
    grid.validate();
    FroxelUniform u{};
    const glm::mat4 v = cam.view(), iv = cam.invView();
    std::memcpy(u.view, glm::value_ptr(v), sizeof(u.view));
    std::memcpy(u.invView, glm::value_ptr(iv), sizeof(u.invView));
    u.tanHalfFovY = std::tan(cam.fovY * 0.5f);
    u.aspect = cam.aspect;
    u.zNear = cam.zNear;
    u.zFar = cam.zFar;
    u.nx = grid.nx;
    u.ny = grid.ny;
    u.nz = grid.nz;
    return u;
}

}  // namespace ff::camera
