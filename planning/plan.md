# Plan

As of: 2026-11-01. Builds on [research.md](research.md). Items marked **proposal** still need a group decision.

## Research question

When the camera moves, does carrying a simulated detail layer from the old froxel grid into the new one give better smoke on screen than regenerating it, and better than simulating on froxels alone?

## Method: one frame

```
1. World step     coarse grid, Stable Fluids: sources -> advect u -> project -> advect density
2. Froxel step    for every froxel f of the NEW frustum:
                    x      = world position of f (current camera)
                    x_back = x - dt * u_world(x)                  backtrace in world space
                    p      = froxel coordinate of x_back in the PREVIOUS camera
                    if p inside previous frustum:  D(f) = sample(D_prev, p) * decay
                    else (newly visible):          D(f) = seed(x)  from the world grid
                  then add fine-resolution sources
3. Compose        fine density = upsample(world density)(x) + D(f), clamped at 0
4. Render         ray-march the froxel columns front to back
```

Step 2 does transfer and advection with **one** interpolation per frame. Every extra resampling blurs the detail, so we never reproject and advect in separate passes. Because the backtrace happens in world space, camera motion does not show up as fake flow (ByroRedux uses the same idea).

### Terms

- **World grid**: uniform, world-aligned, coarse (CPU prototype 48^3, GPU target 64^3 to 128^3). Holds velocity and density for the whole scene, also off screen.
- **Froxel grid**: aligned with the camera frustum, exponential depth slices (GPU target 160x90x64, CPU prototype about 80x45x32).
- **Detail layer D**: fine density minus upsampled world density. This is the state we transport.
- **Seed**: initial detail for newly visible froxels. MVP: zero. Later: synthesized from world-grid vorticity.

### Variants we compare

| Variant | World grid | Froxel state | When the camera moves |
|---|---|---|---|
| A Uniform | one grid, same memory as B to D | none | nothing to do |
| B Froxel-only | none | full sim (velocity + density) | reproject; lost space is gone (ByroRedux / Woo design) |
| C Hybrid reset | yes | detail layer | detail regenerated from the world grid every frame |
| D Hybrid transport | yes | detail layer | detail transferred, new froxels seeded (**ours**) |

## Evaluation (the report needs numbers)

- **Reference**: high-resolution uniform simulation of the same scene (offline, e.g. 256^3), rendered from the same camera.
- **Camera paths** (scripted, fixed dt = 1/60, deterministic): static, slow pan, fast orbit, dolly in, turn away and back.
- **Metrics**
  - image error against the reference per frame (PSNR, SSIM or FLIP on the smoke layer)
  - temporal flicker: frame-to-frame difference compared with the reference's own frame-to-frame difference
  - detail kept: high-frequency energy of the visible density over time
  - mass drift in view across transfers
  - memory (MB) and GPU time per pass (ms, wgpu-profiler)
- **Hypotheses**
  - H1: D beats C on flicker and detail kept under camera motion.
  - H2: D beats B on "turn away and back", because the world grid keeps what left the view.
  - H3: D matches or beats A on visible error at equal memory.

## Milestones

| Date | Course milestone | Our target |
|---|---|---|
| 10.11.2026 | First experiments | CPU 3D Stable Fluids MVP runs (density moves, projection lowers divergence). wgpu compute "hello world" runs on every team machine. World-to-froxel mapping written and unit-tested. |
| 24.11.2026 | Core feature works | World grid on the GPU. Froxel grid with detail layer, static camera. Transfer kernel works with a moving camera (debug slice view). |
| 15.12.2026 | Blockout prototype | Full pipeline with ray-marching, egui panel, scripted camera paths, toggle between variants C and D. |
| 19.01.2027 | Full demo | Variants A to D, reference simulation, evaluation harness writes all metrics to CSV. |
| 02.02.2027 | Final presentation | Results, plots, live demo. |
| 31.03.2027 | Report in ILIAS | Paper-style report with numerical evaluation. |

## Workstreams (proposal, five people)

| Workstream | Owner |
|---|---|
| World grid solver (Stable Fluids, CPU then GPU) | Jonas |
| Froxel grid and transfer (the research core) | TBD |
| Rendering: ray-march, depth composite, egui | TBD |
| Evaluation: reference sim, camera paths, metrics | TBD |
| Detail model, seeding, GPU profiling, build setup | TBD |

## Risks

| Risk | Mitigation |
|---|---|
| Repeated resampling blurs the detail | one interpolation per frame; try MacCormack/BFECC on the transfer |
| Depth slices too coarse up close (ByroRedux sees blocky flames) | tune the depth distribution; report it as a limitation |
| Rust is new to most of us | CPU prototype first; fallback TypeScript + WebGPU with the same WGSL |
| Contribution is narrow | measure honestly; B vs D and C vs D are results either way |

## Open decisions for the group

- Language: Rust + wgpu (see [tech-stack.md](tech-stack.md)) or C++. One objection for C++ came up; decide before 10.11.
- Detail layer: density only (proposal for the MVP), or density plus detail velocity.
- Coupling: one-way world to froxel (proposal), two-way as an extension.
- Seed for newly visible froxels: zero (MVP) or synthesized from vorticity.
- Reference resolution and whether it runs on the GPU.
- Owners for the four open workstreams.
