# Shared WebGPU library

`ff_gpu` provides the GPU context and queue, move-only WebGPU handles, buffers, compute passes and optional GPU timing. The froxel grid and turbulence modules use the same device.

Add the module before its consumers in the parent CMake project:

```cmake
add_subdirectory(gpu)
add_subdirectory(froxel_grid)          # requires glm::glm
add_subdirectory(turbulence_synthesis)
```

Link a consumer with `target_link_libraries(my_target PRIVATE ff_gpu)`. The module requires C++20 and CMake 3.25 or later. It reuses a compatible `webgpu` target or fetches the pinned [WebGPU-distribution](https://github.com/eliemichel/WebGPU-distribution/tree/17dcd42a7683355e7a40ac4e97e77f36dff5b5ab) for wgpu-native v24.0.3.1. When using that distribution, call `target_copy_webgpu_binaries(my_executable)` for executables.

`GpuTimer` returns native timestamp ticks when supported. To obtain milliseconds, supply the selected device's verified nanoseconds per tick to the constructor or set `FF_GPU_TIMESTAMP_PERIOD_NS`.
