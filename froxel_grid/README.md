# Froxel Grid: W2 module

Reusable camera-aligned grid geometry, geometric history, and sampling of a shared world-density texture into one scalar volume per camera. C++20 / WGSL. this directory contains the integration module

## Integrate with team project

The parent supplies the existing shared `ff_gpu` target and `glm::glm` (tested with the group's wgpu-native v24.0.3.1 and GLM 1.0.3). The required common GPU headers are `gpu/context.hpp`, `gpu/handle.hpp`, `gpu/timer.hpp` and `gpu/verify.hpp`.

After defining these dependencies in the parent:

```cmake
add_subdirectory(froxel_grid)
target_link_libraries(your_consumer PRIVATE ff_froxel_gpu)
```

This module creates `ff_camera` and `ff_froxel_gpu`. Replace the previous `ff_camera` definition with this module so there is exactly one implementation of the camera classes. Existing team targets may continue linking `ff_camera`. If the common `ff_gpu` target already lists `float_volume.cpp`, remove that entry: it is compiled by this module. An existing `ff_froxel_gpu` definition must likewise be replaced. CMake rejects these duplicate target/source configurations rather than silently using stale camera code.

The shared GPU target must expose its include directories and WebGPU dependency. No `ff_detail`, world solver, Rust code, renderer or turbulence module is required. The generated WGSL header uses its own directory and namespace, so it cannot overwrite the parent's shader header. This module is not a standalone executable and does not fetch or create a second WebGPU device.

## Use

```cpp
#include "gpu/froxel_batch.hpp"
#include <array>

using namespace ff::gpu;
// ctx is the parent's GpuContext; all textures and encoder use that same device.
auto world = FloatVolume::share(ctx, worldTexture, "shared world density");
ff::camera::FroxelGrid grid{160, 90, 64};
WorldGridGeometry geometry{worldOrigin, worldCellSize};
FloatVolume output(ctx, {grid.nx, grid.ny, grid.nz}, "camera density");
FroxelDensityBatch sampler(ctx);

// Each frame, after recording the upstream world update:
const std::array<FroxelDensityJob, 1> jobs{{
    {camera, grid, &world, geometry, &output}
}};
sampler.encode(ctx, encoder, jobs);
// Record downstream passes reading output.view(), then submit the parent encoder.
```

Keep the wrapper, sampler and output volume across frames. `encode` records one pass per camera, with immutable parameter snapshots; it does not submit, wait or read back. World texels are read when commands execute. `dispatch` is the convenience variant that creates an encoder and submits the batch. The same world may feed several jobs; each needs a distinct output that does not alias any input. Sharing retains the texture reference, without copying texels through the CPU. The producer must not destroy a texture while it is still in use.

World input is a cell-centered 3D `R32Float` texture, one mip/sample, dimensions 1..256 per axis, with `TextureBinding` usage and finite scalar density. Supply the physical lower corner and a scalar cell size. Outputs need `StorageBinding`; the module's volume constructor provides the necessary usages. The producer supplies its own texture or uploads x-fastest scalar values through `FloatVolume::upload`.

## Coordinates and history

- `FroxelGrid::froxelCenter` / `froxelToWorld` / `worldToFroxel` provide CPU mapping. `withFroxelMapping` exposes the matching WGSL helpers to downstream shaders.
- Right-handed world, +Y up, view direction -Z. Depth is axial `-view.z`; `d(k)=near*(far/near)^(k/nz)`. Cell centers use `(i+0.5,j+0.5,k+0.5)`.
- `CameraHistory` and `withFroxelHistory` provide current-to-previous geometric coordinates and interpolation support. Keep each camera's previous scalar data synchronized with its previous camera/grid snapshot; reset history after a cut, reallocation or missing data.
- `inside` uses half-open index bounds `[0,n)`. Previous-field interpolation additionally requires the closed center hull `[0.5,n-0.5]`. Do not treat those two checks as interchangeable.
- `FroxelUniform` is 160 bytes; `HistoryUniform` is 336 bytes. CPU buffers use `i + nx*(j + ny*k)`.
- World density is zero outside its closed physical box and clamped to boundary cell centers inside. A previous-history query uses its own stricter support rule.

W2 supplies no fluid stepping, MAC-velocity sampling, physical backtrace, detail transport, seeding policy or lighting. Those remain upstream/downstream responsibilities.

## GPU parameters

The density pass uses 4x4x4 threads. The batch parameter stride is 512 bytes per job, with camera parameters at offset 0 and world geometry at offset 256.

Source papers for this froxel grid module:

- [Woo 2006, §3.1](https://www.cs.ubc.ca/~rbridson/docs/awoo_msc.pdf): frustum-grid concept and world/grid transformations. Woo's depth distribution and full fluid solver are not reproduced here.
- [Wroński 2014, slide 25](https://bartwronski.com/wp-content/uploads/2014/08/bwronski_volumetric_fog_siggraph2014.pdf): view-aligned 3D texture, exponential depth and the 160x90x64 layout. The exact near/far formula and boundary contracts are project choices.
- [Olsson et al. 2012, §3.1](https://doi.org/10.2312/EGGH/HPG12/087-096): mathematical context for exponential layers/logarithmic inverse; its cluster-shape parameterization differs from this module.
- [Steiner et al. 2024, §§4.1 and 5](https://doi.org/10.1145/3675370): conceptual separation of reprojection coordinates and valid cached support. Their radial depth, sparse NeRF cache and neural method are not implemented.
- [WGSL layout rules](https://www.w3.org/TR/2026/CRD-WGSL-20261106/#address-space-layout-constraints): CPU/GPU layout constraints. Trilinear interpolation is a standard method, implemented with explicit loads because `R32Float` filtering is optional.
