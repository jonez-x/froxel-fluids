# Tech Stack

**Rust + wgpu + WGSL, UI with egui.**

## Why

- One codebase runs natively on the Macs (Metal) and the NVIDIA PCs (Vulkan/DX12).
- Compute shaders and 3D textures, i.e. everything the simulation needs.
- GPU time per pass can be measured with wgpu-profiler.

## Versions (as of October 2026)

| Crate | Version |
|---|---|
| wgpu | 30.0.1 |
| egui / eframe / egui-wgpu | 0.37 |
| wgpu-profiler | 0.28 (uses wgpu 30) |

Pin versions exactly. wgpu changes its API roughly every three months.

## Rejected

- NVIDIA Warp: runs only on the CPU on Macs.
- Taichi: last release July 2025.
- OpenGL compute: macOS only supports OpenGL 4.1.
- Fallback if Rust does not work out: TypeScript + WebGPU in the browser with the same WGSL shaders.

## Watch out for

- On Apple GPUs, timing is only available between passes, so use one compute pass per kernel.
- Read fields from one texture and write to a second one (ping-pong).
- Default limit: 4 storage textures per shader.
