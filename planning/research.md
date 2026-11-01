# Research: What exists, and what do we do?

As of: 2026-11-01 (first version 2026-10-29). Research is closed with this version. Full bibliography with DOIs, check status and search log: [references.md](references.md).

## Summary

Frustum grids, detail on top of a coarse simulation, and reusing froxel data across frames all exist. A **single** froxel grid that is itself the simulation domain and is reprojected from the previous frustum when the camera moves also exists, in a public hobby engine (ByroRedux, 2026). What is still open is the **two-scale** version: a coarse world grid that keeps the whole flow alive, plus a persistent, simulated **detail layer** on the froxel grid that is carried from the old frustum into the new one and re-seeded from the world grid where the camera reveals new space. Nobody has measured what that transport buys over resetting the detail, or over a froxel-only simulation.

## What already exists

| Building block | Exists? | Source |
|---|---|---|
| Smoke simulation directly on a frustum grid | yes, fixed camera only | Woo 2006 |
| Frustum detail grid with noise over a coarse simulation, one-way | yes; the patent does not say whether detail is kept across frames (patented) | Lucasfilm, US 8,725,476 |
| Camera frustum as simulation window | yes, fixed per shot | Pixar, US 9,367,956 |
| Resolution that follows the viewer | yes, with world-axis-aligned grids | Gao et al. 2013 |
| Simulation grid that moves, state carried along | yes, axis-aligned, follows the flow or objects, not a perspective grid | Shah et al. 2004, Patel et al. 2005, Zhu et al. 2013 |
| View-dependent adaptive octree for smoke | yes, world axes, refined by the frustum | Bunlutangtum & Kanongchaiyos 2011 |
| Frustum up-res in film production | yes, stateless ("frame independent") | ILM, Fast Fluid Up-Res 2025 |
| Curl noise / detail from the coarse flow | yes | Bridson 2007, Kim 2008 |
| Reusing froxel data across frames | yes, for fog lighting | Unreal, Godot, Frostbite |
| **Froxel grid as the only simulation domain, reprojected through the previous frustum** | **yes, public code, single-level** | **ByroRedux (GitHub, 2026)** |
| Gaze-dependent fluid simulation (VR) | yes, particles, adaptive time steps, no frustum grid | Wang et al. 2024, 2025, 2026 |
| Coarse world grid + transported froxel detail layer | not found | this check |

## The three checks from 2026-10-29

1. **"Temporal Foveated Fluid Animation in Virtual Reality"** (Wang et al., IEEE TVCG 2026). Read at abstract level (full text is behind IEEE). It adapts *time steps* by gaze eccentricity in an asynchronous position-based fluids (particle) solver. No grid, no frustum, no state transfer between views. Not a conflict. The same holds for its two predecessors (IEEE VR 2024, TVCG 2025).
2. **"Cited by" Woo 2006 and Gao 2013.** Gao 2013 (Semantic Scholar): 21 citing works, none with a camera-aligned grid that carries state. Woo 2006: Google Scholar shows no "cited by" entry at all, Semantic Scholar one unrelated thesis (2010). Woo's own conclusion calls the fixed frustum "a limitation and a topic for future work" and suggests a moving camera may need particles. Found separately: Shah et al. 2004, Patel et al. 2005 and Zhu et al. 2013 move simulation grids and interpolate the state onto the new grid, but the grids are axis-aligned and follow the flow or objects. Bunlutangtum & Kanongchaiyos 2011 refine a world octree by the view frustum (same class as Gao 2013).
3. **GitHub code search "froxel" + advect / divergence / pressure / jacobi.** About 40 hits per query, almost all fog-lighting renderers that rebuild froxels every frame or sample a world-space fog sim. One real overlap: **ByroRedux** (`matiaszanolli/ByroRedux`). Its fire and smoke solver runs inside a 160x90x64 froxel grid; each froxel reconstructs its world position and backtraces (RK2) into the previous camera's froxel volumes, so camera motion is not mistaken for flow. There is no world grid: whatever leaves the frustum is lost, and the developers report blocky flames up close and are moving fire into world-space boxes. Shadertoy blocks automated access with a bot check; still to do by hand.

## What we do

Woo 2006 names a moving camera as future work; we take that step with a different design. A coarse world grid (Stable Fluids) holds the whole flow, on and off screen. A camera-aligned froxel grid holds a persistent **detail layer**: the difference between fine and upsampled coarse density, advected and kept from frame to frame. When the camera moves, each new froxel looks up its value in the previous frustum. Froxels that were not visible before are seeded from the world grid. We compare four variants: uniform grid at equal memory, froxel-only (the ByroRedux/Woo design), hybrid with reset, hybrid with transport.

If the detail is pure, stateless noise, there is nothing to transport, and then there is no contribution.

## How we phrase it

> Froxel-only simulation with reprojection exists. To our knowledge, no prior work couples a coarse world-space simulation with a persistent detail layer on a camera-aligned froxel grid that is transferred between frusta and re-seeded from the world grid where new space becomes visible, and none measures what this transfer gains over resetting the detail or over a froxel-only simulation.

Do not claim: first frustum grid, first detail over a coarse simulation, first froxel reprojection, **first simulation on a moving froxel grid**.

Cite ByroRedux openly in the report and use its design as the froxel-only baseline.

## Sources

See [references.md](references.md) for every source with DOI, what we use it for and how far we checked it.
