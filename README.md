# Froxel Fluids

Hybrid smoke simulation: a coarse world grid (Stable Fluids) plus a camera-aligned froxel grid for fine detail. Research focus: transporting the simulated detail state between camera frusta when the camera moves.

Computer Graphics Project, University of Tübingen, Group G04.

Research and tech stack: see [planning/](planning/).

Status (2026-11-19): C++20 + WebGPU, built with CMake from the repository root. Layout and versions: [planning/tech-stack.md](planning/tech-stack.md).

| Folder | What | Owner |
|---|---|---|
| [world_solver/](world_solver/) | W1 world grid, Stable Fluids on the CPU (C++ port of the Rust prototype) | Jonas |
| [froxel_grid/](froxel_grid/README.md) | W2 camera/grid geometry, geometric history, GPU density sampling | Andreas |
| [turbulence_synthesis/](turbulence_synthesis/) | W5 procedural sub-grid detail per camera, CPU reference and GPU | |
| [gpu/](gpu/README.md) | shared WebGPU infrastructure `ff_gpu`, **not in the repository yet** | |
| [app/](app/README.md) | the integrated program | |
| [rust-cpu-prototype/](rust-cpu-prototype/) | original Rust CPU prototype, kept as reference | Jonas |

```
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
./build/world_solver/world_demo
```

Without `ff_gpu`, only `world_solver/` is built; the root CMake skips the other modules with a warning.
