# gpu/ (ff_gpu): shared WebGPU infrastructure

Not in the repository yet. `froxel_grid/` and `turbulence_synthesis/` were developed against an existing
`ff_gpu` target outside this repository; the root `CMakeLists.txt` skips both modules (and `app/`) until
this folder has a `CMakeLists.txt` that defines `ff_gpu`.

What the modules use (collected from their sources):

- WebGPU: wgpu-native v24.0.3.1, C API (`wgpu*` functions, `WGPU*` types). CMake function
  `target_copy_webgpu_binaries` (optional, used by `turbulence_gpu_tests`).
- `gpu/context.hpp`: `ff::gpu::GpuContext` with `create()` (throws `ff::gpu::NoAdapterError` without a GPU),
  `device()`, `queue()`, `waitUntil(...)`.
- `gpu/buffer.hpp`: `ff::gpu::GpuBuffer`.
- `gpu/timer.hpp`: `ff::gpu::GpuTimer` (GPU time per pass).
- `gpu/handle.hpp`: RAII handles `ShaderModule`, `PipelineLayout`, `ComputePipeline`, `BindGroupLayout`,
  `BindGroup`, `CommandEncoder`, `CommandBuffer`, `ComputePassEncoder`; `ff::gpu::sv(...)` (string view to
  `WGPUStringView`).
- `gpu/verify.hpp`.
- `float_volume.cpp` must **not** be part of `ff_gpu`; `froxel_grid` compiles it.
