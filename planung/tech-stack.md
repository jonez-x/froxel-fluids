# Tech-Stack

**Rust + wgpu + WGSL, UI mit egui.**

## Warum

- Eine Codebasis läuft nativ auf den Macs (Metal) und den NVIDIA-PCs (Vulkan/DX12).
- Compute-Shader und 3D-Texturen, also alles, was die Simulation braucht.
- GPU-Zeit pro Pass messbar mit wgpu-profiler.

## Versionen (Stand Oktober 2026)

| Crate | Version |
|---|---|
| wgpu | 30.0.1 |
| egui / eframe / egui-wgpu | 0.37 |
| wgpu-profiler | 0.28 (nutzt wgpu 30) |

Versionen exakt pinnen. wgpu ändert die API etwa alle drei Monate.

## Verworfen

- NVIDIA Warp: läuft auf dem Mac nur auf der CPU.
- Taichi: letztes Release Juli 2025.
- OpenGL-Compute: macOS kann nur OpenGL 4.1.
- Fallback, falls Rust nicht klappt: TypeScript + WebGPU im Browser mit denselben WGSL-Shadern.

## Worauf achten

- Auf Apple-GPUs gibt es Zeitmessung nur zwischen Passes, also einen Compute-Pass pro Kernel.
- Felder aus einer Textur lesen und in eine zweite schreiben (Ping-Pong).
- Standardlimit: 4 Storage-Texturen pro Shader.
