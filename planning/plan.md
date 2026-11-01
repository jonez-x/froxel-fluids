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
| P Particles + froxel pressure | yes | none; detail on world-space particles, froxel grid only for pressure and splatting | nothing to transfer, particles stay in world space |

Variant P comes from supervisor feedback (2026-11-01: "look into SPH too if froxel is too crude"). Pressure from a grid with particles carrying the state is PIC/FLIP rather than classic SPH (Zhu & Bridson 2005); coarse grid plus detail particles also exists (Pfaff et al. 2010, Selle et al. 2005). P is the Lagrangian answer to the question D answers on the grid, so comparing D and P is a result either way.

**Decision gate 24.11.2026:** if the first transfer test shows D visibly smearing detail under camera motion, P becomes the main variant and D the baseline. Otherwise P is an extension after the blockout (15.12.).

## Transfer extensions (what separates D from ByroRedux)

ByroRedux has a froxel-only grid: no world grid, a plain resampling transfer, nothing outside the view. Each extension below needs our world grid, and each is a switch, so the evaluation can run D with and without it. The mechanisms exist elsewhere (cited); applying them to a detail layer carried between frusta is what we did not find.

| ID | Extension | Idea | Prior art for the mechanism | Measured by |
|---|---|---|---|---|
| T1 | Conservative residual transfer | Detail is a residual: its sum over every world cell must stay zero. After each transfer, correct per world cell. Coarse and fine stay consistent, no mass drift. | multigrid restriction; Chentanez & Müller 2014 (mass-conserving Eulerian) | mass drift, image error |
| T2 | Scale-aware transfer | Dolly in: froxels get finer than the history can fill, so synthesize the missing band (curl noise driven by world-grid vorticity). Dolly out: low-pass before transfer to avoid aliasing. | Kim et al. 2008, Narain et al. 2008 | detail kept, flicker on the dolly path |
| T3 | Off-screen detail memory | When detail leaves the frustum, store its statistics (energy, vorticity) per world cell; when the camera turns back, rebuild detail from them instead of inventing it. | energy transport: Narain et al. 2008, Pfaff et al. 2010 | "turn away and back" path (H2) |
| T4 | Guard band | Simulate a slightly larger frustum, widened in the direction the camera moves, so incoming froxels already have history. | guard bands in rendering | edge flicker and error |
| T5 | Confidence per froxel | Count how often a froxel's detail was resampled; blend toward the world grid as confidence drops. Bounds the blur from repeated resampling. | history rejection in temporal anti-aliasing | detail kept vs. blur over time |

Priority: T1 and T3 are the core (they only work with a world grid). T2 if time allows. T4 and T5 are small switches.

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
  - H4: each of T1 to T5 improves its target metric over plain D (ablation).
  - H5: open question, D vs P: which keeps more detail per millisecond under camera motion.

## Milestones

| Date | Course milestone | Our target |
|---|---|---|
| 10.11.2026 | First experiments | CPU 3D Stable Fluids MVP runs (density moves, projection lowers divergence). wgpu compute "hello world" runs on every team machine. World-to-froxel mapping written and unit-tested. |
| 24.11.2026 | Core feature works | World grid on the GPU. Froxel grid with detail layer, static camera. Transfer kernel works with a moving camera (debug slice view). **Decision gate D vs P.** |
| 15.12.2026 | Blockout prototype | Full pipeline with ray-marching, egui panel, scripted camera paths, toggle between variants C and D. T1 and T4 in. |
| 19.01.2027 | Full demo | Variants A to D (and P if chosen), T3 and T5, reference simulation, evaluation harness writes all metrics to CSV. T2 if time allows. |
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
| Repeated resampling blurs the detail | one interpolation per frame; T5; MacCormack/BFECC on the transfer; fallback variant P |
| Depth slices too coarse up close (ByroRedux sees blocky flames) | tune the depth distribution; report it as a limitation |
| Rust is new to most of us | CPU prototype first; fallback TypeScript + WebGPU with the same WGSL |
| Contribution is narrow | measure honestly; B vs D and C vs D are results either way |

## Open decisions for the group

- Language: Rust + wgpu (see [tech-stack.md](tech-stack.md)) or C++. One objection for C++ came up; decide before 10.11.
- Detail layer: density only (proposal for the MVP), or density plus detail velocity.
- Coupling: one-way world to froxel (proposal), two-way as an extension.
- Seed for newly visible froxels: zero (MVP) or synthesized from vorticity.
- Reference resolution and whether it runs on the GPU.
- Ask the supervisor: SPH (pressure from particle neighbours) or PIC/FLIP (pressure from a grid) for variant P?
- Owners for the four open workstreams.
