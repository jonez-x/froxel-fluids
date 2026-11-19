// Per-camera geometric history, independent of scalar storage and W6 transport.
#pragma once

#include "camera/froxel_grid.hpp"

#include <optional>

namespace ff::camera {

struct CameraSnapshot {
    Camera camera;
    FroxelGrid grid;
};

// Advance once per camera/frame. Scalar data must match the snapshots and be fully populated.
// Callers explicitly cut/reset on scene changes or absent/reallocated previous scalar data.
class CameraHistory {
public:
    // Validates before changing state. First advance, a cut, or changed grid dimensions
    // stores the new current state without previous history. The next advance can reuse it.
    // Motion and projection changes retain the actual old camera parameters.
    void advance(const Camera& camera, const FroxelGrid& grid, bool cameraCut = false);
    void reset() noexcept;

    bool hasCurrent() const noexcept { return current_.has_value(); }
    bool hasHistory() const noexcept { return previous_.has_value(); }
    bool identityMapping() const noexcept { return identityMapping_; }
    const CameraSnapshot& current() const;  // throws std::logic_error before the first advance/after reset
    const std::optional<CameraSnapshot>& previous() const noexcept { return previous_; }

private:
    std::optional<CameraSnapshot> current_, previous_;
    bool identityMapping_ = false;
};

struct HistoryCoord {
    glm::vec3 uvw{0.0f};  // continuous coordinate in the previous grid
    bool inside = false;  // finite half-open cell-index validity
    bool sampleable = false;  // inside the closed hull of previous cell centers on all axes
    bool hasHistory = false;  // global snapshot availability, also true for an invalid point query
};

struct TrilinearStencil {
    glm::uvec3 lo{0}, hi{0};
    glm::vec3 weights{0.0f};  // weight of hi on each axis; lo has weight 1-weights
    bool valid = false;
};

// No clamped extrapolation: support is exactly [0.5, n-0.5] on every axis.
// The last center duplicates its upper neighbor; a singleton axis supports only 0.5.
TrilinearStencil historyStencil(const FroxelGrid& grid, glm::vec3 uvw);

// Geometry only: no scalar read, temporal blend, velocity integration or history transport.
// Finite current coordinates may extrapolate. An exact camera/grid match preserves uvw
// rather than round-tripping edge centers through world space. Invalid points return
// zero coordinates with inside/sampleable false, while preserving global hasHistory.
HistoryCoord reprojectFroxel(const CameraHistory& history, glm::vec3 currentUVW);
HistoryCoord reprojectCenter(const CameraHistory& history, uint32_t i, uint32_t j, uint32_t k);

// Accepts W6's future world-space backtrace directly, for one subsequent scalar resample.
// Always maps the supplied point; never applies the coordinate identity shortcut.
HistoryCoord worldToPrevious(const CameraHistory& history, glm::vec3 world);

struct alignas(16) HistoryUniform {
    FroxelUniform current;
    FroxelUniform previous;
    uint32_t hasHistory = 0;
    uint32_t identityMapping = 0;
    uint32_t pad[2] = {0, 0};
};
static_assert(std::is_standard_layout_v<HistoryUniform>);
static_assert(alignof(HistoryUniform) == 16 && sizeof(HistoryUniform) == 336);
static_assert(offsetof(HistoryUniform, current) == 0);
static_assert(offsetof(HistoryUniform, previous) == 160);
static_assert(offsetof(HistoryUniform, hasHistory) == 320);
static_assert(offsetof(HistoryUniform, identityMapping) == 324);
static_assert(offsetof(HistoryUniform, pad) == 328);

// Requires a current snapshot. With no history, previous contains current parameters
// as a safe placeholder; hasHistory remains zero and forbids reading previous scalar data.
HistoryUniform makeHistoryUniform(const CameraHistory& history);

}  // namespace ff::camera
