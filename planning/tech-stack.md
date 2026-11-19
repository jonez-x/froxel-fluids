# Tech Stack

**C++20 + WebGPU (wgpu-native) + WGSL, built with CMake.** Decided by the group in November 2026; replaces the earlier Rust + wgpu choice.

## Why

- WebGPU runs natively on the Macs (Metal) and the NVIDIA PCs (Vulkan/DX12).
- Compute shaders and 3D textures, i.e. everything the simulation needs.
- The WGSL shaders stay the same as in the Rust plan.

## Versions

| Dependency | Version | Used by |
|---|---|---|
| CMake | >= 3.25 | all |
| wgpu-native | v24.0.3.1 | `ff_gpu` |
| GLM | 1.0.3 | `froxel_grid`, `turbulence_synthesis` |
| doctest | 2.4.12 | tests |
| raylib | 5.5 | `world_demo` (CPU view of the world solver) |

Pin versions exactly (`cmake/dependencies.cmake`). wgpu-native changes its API often.

## Project layout

The root `CMakeLists.txt` fetches the dependencies and adds the modules in dependency order. Each module has its own folder and `CMakeLists.txt` and expects the parent to provide `glm::glm`, `doctest::doctest` and `ff_gpu`.

| Folder | Workstream | Targets |
|---|---|---|
| `world_solver/` | W1 world grid | `ff_world`, `world_demo`, `world_tests` |
| `froxel_grid/` | W2 froxel grid | `ff_camera`, `ff_froxel_gpu` |
| `turbulence_synthesis/` | W5 detail | `ff_froxel_link`, `ff_detail`, `ff_detail_gpu` |
| `gpu/` | shared | `ff_gpu` (missing, see `gpu/README.md`) |
| `app/` | integration | the program |

Build: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build`.

## Rejected

- Rust + wgpu: the first choice (see git history); the group moved to C++. The Rust CPU prototype was ported 1:1 to `world_solver/` and then removed (still in the git history).
- NVIDIA Warp: runs only on the CPU on Macs.
- Taichi: last release July 2025.
- OpenGL compute: macOS only supports OpenGL 4.1.

## Watch out for

- On Apple GPUs, timing is only available between passes, so use one compute pass per kernel.
- Read fields from one texture and write to a second one (ping-pong).
- Default limit: 4 storage textures per shader.
- `R32Float` filtering is optional in WebGPU, so trilinear sampling is done with explicit loads.
