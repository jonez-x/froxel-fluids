# Research: What exists, and what do we do?

As of: 2026-10-29

## Summary

Frustum grids, detail on top of a coarse simulation, and reusing froxel data across frames all exist already. What is open is a narrow case: carrying a **simulated detail state** on the froxel grid **from the old frustum into the new one** when the camera moves, instead of regenerating it every frame. Woo (2006) names exactly this as open future work.

## What already exists

| Building block | Exists? | Source |
|---|---|---|
| Smoke simulation directly on a frustum grid | yes, fixed camera only | Woo 2006 |
| Frustum detail grid with noise over a coarse simulation, one-way | yes, recomputed every frame (patented) | Lucasfilm, US 8,725,476 |
| Camera frustum as simulation window | yes, fixed per shot | Pixar, US 9,367,956 |
| Resolution that follows the viewer | yes, with world-axis-aligned grids | Gao et al. 2013 |
| Frustum up-res in film production | yes, stateless ("frame independent") | ILM, Fast Fluid Up-Res 2025 |
| Curl noise / detail from the coarse flow | yes | Bridson 2007, Kim 2008 |
| Reusing froxel data across frames | yes, but only for fog lighting | Unreal, Godot, Frostbite |
| Public code for froxel fluid simulation | not found | GitHub search |

## What we do

A coarse world grid (Stable Fluids) plus a camera-aligned froxel grid with its own persistent detail (fine density and a detail state). When the camera moves, this state is transferred into the new frustum. Newly visible froxels start from the world grid. We compare transport against reset.

Important: if the detail is pure, stateless noise, there is nothing to transport, and then there is no contribution.

## How we phrase it

> To our knowledge, no prior work maintains a persistent, simulated detail state on a camera-aligned froxel grid and explicitly transfers it from the previous to the new frustum as the camera moves.

Do not claim: first frustum grid, first detail over a coarse simulation, first froxel reprojection.

## Still to check before we say "novel"

- Read "Temporal Foveated Fluid Animation in Virtual Reality" (IEEE TVCG 2026) in full. It is the only recent hit with a temporal aspect.
- Go through Google Scholar "Cited by" for Woo 2006 and Gao 2013.
- Search GitHub code and Shadertoy for "froxel" + "advect".

## Sources

- Woo 2006, Realistic Smoke Simulation Using a Frustum Aligned Grid: https://www.cs.ubc.ca/~rbridson/docs/awoo_msc.pdf
- US 8,725,476 (Lucasfilm): https://patents.google.com/patent/US8725476B1/en
- US 9,367,956 (Pixar): https://patents.google.com/patent/US9367956B2/en
- Gao et al. 2013, View-Dependent Multiscale Fluid Simulation: https://cg.cs.tsinghua.edu.cn/papers/TR_110830_gy.pdf
- ILM, Fast Fluid Up-Res (SIGGRAPH 2025 Talk): https://history.siggraph.org/?p=227366
- Kim et al. 2008, Wavelet Turbulence: https://www.cs.cornell.edu/~tedkim/WTURB
- Unreal Volumetric Fog: https://docs.unrealengine.com/4.27/BuildingWorlds/FogEffects/VolumetricFog/
