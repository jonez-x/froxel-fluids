#include "camera/history.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ff::camera {
namespace {

bool finite(glm::vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }

bool sameResolution(const FroxelGrid& a, const FroxelGrid& b) {
    return a.nx == b.nx && a.ny == b.ny && a.nz == b.nz;
}

bool sameMapping(const CameraSnapshot& a, const CameraSnapshot& b) {
    const auto& x = a.camera;
    const auto& y = b.camera;
    return sameResolution(a.grid, b.grid) && x.position.x == y.position.x && x.position.y == y.position.y &&
           x.position.z == y.position.z && x.yaw == y.yaw && x.pitch == y.pitch && x.fovY == y.fovY &&
           x.aspect == y.aspect && x.zNear == y.zNear && x.zFar == y.zFar;
}

HistoryCoord fromCoordinate(const FroxelGrid& previousGrid, FroxelCoord coordinate) {
    HistoryCoord result;
    result.hasHistory = true;
    result.uvw = coordinate.uvw;
    result.inside = coordinate.inside;
    result.sampleable = coordinate.inside && historyStencil(previousGrid, coordinate.uvw).valid;
    return result;
}

}  // namespace

void CameraHistory::advance(const Camera& camera, const FroxelGrid& grid, bool cameraCut) {
    // In particular, validate finite view-matrix translation before mutating either snapshot.
    (void)makeUniform(camera, grid);
    const CameraSnapshot next{camera, grid};
    std::optional<CameraSnapshot> nextPrevious;
    if (current_ && !cameraCut && sameResolution(current_->grid, grid)) nextPrevious = current_;
    const bool nextIdentity = nextPrevious && sameMapping(next, *nextPrevious);
    current_ = next;
    previous_ = nextPrevious;
    identityMapping_ = nextIdentity;
}

void CameraHistory::reset() noexcept {
    current_.reset();
    previous_.reset();
    identityMapping_ = false;
}

const CameraSnapshot& CameraHistory::current() const {
    if (!current_) throw std::logic_error("camera history has no current snapshot");
    return *current_;
}

TrilinearStencil historyStencil(const FroxelGrid& grid, glm::vec3 uvw) {
    grid.validate();
    TrilinearStencil result;
    if (!finite(uvw)) return result;
    const uint32_t dims[3] = {grid.nx, grid.ny, grid.nz};
    for (int axis = 0; axis < 3; ++axis) {
        if (uvw[axis] < 0.5f || uvw[axis] > float(dims[axis]) - 0.5f) return result;
    }
    for (int axis = 0; axis < 3; ++axis) {
        const float centered = uvw[axis] - 0.5f;
        result.lo[axis] = uint32_t(std::floor(centered));
        result.hi[axis] = std::min(result.lo[axis] + 1u, dims[axis] - 1u);
        result.weights[axis] = centered - float(result.lo[axis]);
    }
    result.valid = true;
    return result;
}

HistoryCoord worldToPrevious(const CameraHistory& history, glm::vec3 world) {
    HistoryCoord result;
    result.hasHistory = history.hasHistory();
    if (!result.hasHistory || !finite(world)) return result;
    const auto& previous = *history.previous();
    return fromCoordinate(previous.grid, previous.grid.worldToFroxel(previous.camera, world));
}

HistoryCoord reprojectFroxel(const CameraHistory& history, glm::vec3 currentUVW) {
    HistoryCoord result;
    result.hasHistory = history.hasHistory();
    if (!result.hasHistory || !finite(currentUVW)) return result;
    if (history.identityMapping()) {
        const auto& grid = history.current().grid;
        const bool inside = currentUVW.x >= 0.0f && currentUVW.x < float(grid.nx) && currentUVW.y >= 0.0f &&
                            currentUVW.y < float(grid.ny) && currentUVW.z >= 0.0f && currentUVW.z < float(grid.nz);
        return fromCoordinate(grid, {currentUVW, inside});
    }
    const auto& current = history.current();
    try {
        return worldToPrevious(history, current.grid.froxelToWorld(current.camera, currentUVW));
    } catch (const std::overflow_error&) {
        return result;  // no finite world point to map; snapshot availability is unchanged
    }
}

HistoryCoord reprojectCenter(const CameraHistory& history, uint32_t i, uint32_t j, uint32_t k) {
    if (!history.hasCurrent()) return {};
    const auto& grid = history.current().grid;
    if (i >= grid.nx || j >= grid.ny || k >= grid.nz) throw std::out_of_range("history center index outside current grid");
    return reprojectFroxel(history, {float(i) + 0.5f, float(j) + 0.5f, float(k) + 0.5f});
}

HistoryUniform makeHistoryUniform(const CameraHistory& history) {
    const auto& current = history.current();
    HistoryUniform uniform{};
    uniform.current = makeUniform(current.camera, current.grid);
    uniform.previous = uniform.current;
    if (history.hasHistory()) {
        const auto& previous = *history.previous();
        uniform.previous = makeUniform(previous.camera, previous.grid);
        uniform.hasHistory = 1;
        uniform.identityMapping = history.identityMapping() ? 1u : 0u;
    }
    return uniform;
}

}  // namespace ff::camera
