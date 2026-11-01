# References

As of: 2026-11-01. Every DOI below was resolved through Crossref or the publisher page on this date.

**Checked** says how far we read a source ourselves:
- **full**: the original was read, at least the parts we cite.
- **abstract**: only the abstract or publisher summary.
- **meta**: only title, authors, venue and DOI. Read before citing anything specific from it.

## A. Foundations

| Key | Reference | DOI / link | Why we cite it | Checked |
|---|---|---|---|---|
| Stam99 | J. Stam. Stable Fluids. SIGGRAPH 1999, 121–128. | [10.1145/311535.311548](https://doi.org/10.1145/311535.311548) | World-grid solver | full (Jonas) |
| Fedkiw01 | R. Fedkiw, J. Stam, H. W. Jensen. Visual Simulation of Smoke. SIGGRAPH 2001. | [10.1145/383259.383260](https://doi.org/10.1145/383259.383260) | Smoke model, vorticity confinement | meta |
| Bridson15 | R. Bridson. Fluid Simulation for Computer Graphics, 2nd ed. CRC Press, 2015. | [10.1201/9781315266008](https://doi.org/10.1201/9781315266008) | Textbook reference (named in course slides) | meta |

## B. Closest prior work (novelty)

| Key | Reference | DOI / link | What it does, and how we differ | Checked |
|---|---|---|---|---|
| Woo06 | A. W. L. Woo. Realistic Smoke Simulation Using a Frustum Aligned Grid. MSc thesis, UBC, 2006 (supervisor R. Bridson). | [PDF](https://www.cs.ubc.ca/~rbridson/docs/awoo_msc.pdf) | Smoke simulated on a frustum grid, fixed camera. Calls the fixed frustum "a limitation and a topic for future work"; suggests a moving camera may need particles. No world grid, no transfer. | full (conclusion) |
| Lucasfilm14 | F. Losasso Petterson. Applying details in a simulation. US 8,725,476 B1, Lucasfilm, granted 2014. | [Google Patents](https://patents.google.com/patent/US8725476B1/en) | Coarse simulation plus frustum-shaped detail grid with noise/turbulence. Does not say whether detail is kept across frames or how camera motion is handled. | full (first ~75 %) |
| Pixar16 | M. K. O'Brien, D. Hale, R. Kautzman, J. Chong. Windowed simulation in fluid flows. US 9,367,956 B2, Pixar, 2016. | [Google Patents](https://patents.google.com/patent/US9367956B2/en) | Fine simulation in the camera-frustum window over a coarse one, window set per shot. No transfer under camera motion described. | full (first ~75 %) |
| Gao13 | Y. Gao, C.-F. Li, B. Ren, S.-M. Hu. View-Dependent Multiscale Fluid Simulation. IEEE TVCG 19(2), 2013. | [10.1109/TVCG.2012.117](https://doi.org/10.1109/TVCG.2012.117) | Resolution follows the viewer, world-axis-aligned grids. Closest academic work; differs in grid alignment. | abstract |
| Bunlutangtum11 | R. Bunlutangtum, P. Kanongchaiyos. Adaptive Grid Refinement Using View-Dependent Octree for Grid-Based Smoke Simulation. MIG 2011, LNCS 7060. | [10.1007/978-3-642-25090-3_18](https://doi.org/10.1007/978-3-642-25090-3_18) | World octree refined by frustum and flow criteria. Same class as Gao13. Follow-up: VRCAI 2011, [10.1145/2087756.2087832](https://doi.org/10.1145/2087756.2087832). | abstract |
| Shah04 | M. Shah, J. M. Cohen, S. Patel, P. Lee, F. Pighin. Extended Galilean Invariance for Adaptive Fluid Simulation. SCA 2004. | [10.1145/1028523.1028551](https://doi.org/10.1145/1028523.1028551) | Simulation window moves with the flow, state carried along. Not camera-driven. | abstract |
| Patel05 | S. Patel, A. Chu, J. Cohen, F. Pighin. Fluid Simulation Via Disjoint Translating Grids. SIGGRAPH 2005 Sketches. | [10.1145/1187112.1187280](https://doi.org/10.1145/1187112.1187280) | Several translating grids. Not camera-driven. | abstract |
| Zhu13 | B. Zhu, W. Lu, M. Cong, B. Kim, R. Fedkiw. A New Grid Structure for Domain Extension. ACM TOG 32(4), 2013. | [10.1145/2461912.2461999](https://doi.org/10.1145/2461912.2461999) | Fine uniform region inside stretched far-field cells; the fine region moves (objects, flame, "point of camera focus") and velocity is interpolated from the old to the new grid. Axis-aligned, not a perspective grid, no detail layer. | full |
| ILM25 | X. Martin Ramirez, R. Jones. Fast Fluid Up-Res. SIGGRAPH 2025 Talks. | [10.1145/3721239.3734123](https://doi.org/10.1145/3721239.3734123) | Camera-frustum aligned grids for up-res, "frame independent", so no state across frames. | abstract |
| Wang24 | Y. Wang, Y. Zhang, X. Yang. Foveated Fluid Animation in Virtual Reality. IEEE VR 2024. | [10.1109/VR58804.2024.00074](https://doi.org/10.1109/VR58804.2024.00074) | Gaze-dependent particle simulation. | meta |
| Wang25 | Y. Wang, Y. Zhang, X.-H. Yang, H. Wang, X.-B. Yang. Scene-Based Foveated Fluid Animation in Virtual Reality. IEEE TVCG, 2025. | [10.1109/TVCG.2025.3609904](https://doi.org/10.1109/TVCG.2025.3609904) | Gaze- and curvature-dependent particle resolution. No grid. | abstract |
| Wang26 | Y. Wang, Y. Zhang, Z. Wang, Z. Liu, X.-B. Yang. Temporal Foveated Fluid Animation in Virtual Reality. IEEE TVCG, 2026. | [10.1109/TVCG.2026.3680105](https://doi.org/10.1109/TVCG.2026.3680105) | Gaze-dependent time steps, position-based fluids. No grid. | abstract |
| ByroRedux | M. Zanolli et al. ByroRedux engine, volumetrics and combustion solver. GitHub, 2026. | [survey](https://github.com/matiaszanolli/ByroRedux/blob/main/docs/engine/fire-volumetrics-phase0.md), [plan](https://github.com/matiaszanolli/ByroRedux/blob/main/docs/engine/fire-volumetrics-plan.md) | Froxel grid is the only simulation domain; RK2 backtrace through the previous camera's froxel volumes. No world grid. Our baseline B. | full (docs + shader include) |

## C. Froxels and reprojection

| Key | Reference | DOI / link | Why we cite it | Checked |
|---|---|---|---|---|
| Wronski15 | B. Wroński. Volumetric Fog and Lighting. In GPU Pro 6, CRC Press, 2015 (talk: SIGGRAPH 2014 Advances in Real-Time Rendering). | [10.1201/b18805-18](https://doi.org/10.1201/b18805-18) | Origin of froxel volumetric fog (named in course slides) | meta |
| Hillaire15 | S. Hillaire. Physically Based and Unified Volumetric Rendering in Frostbite. SIGGRAPH 2015 Advances in Real-Time Rendering course. | [course page](https://advances.realtimerendering.com/s2015/) | Froxel volumes with temporal reprojection (named in course slides) | meta |
| Nehab07 | D. Nehab, P. V. Sander, J. Lawrence, N. Tatarchuk, J. R. Isidoro. Accelerating Real-Time Shading with Reverse Reprojection Caching. Graphics Hardware 2007. | [10.2312/EGGH/EGGH07/025-036](https://diglib.eg.org/handle/10.2312/EGGH.EGGH07.025-036) | Reprojection itself is prior art; we do not claim it | meta |
| UnrealFog | Epic Games. Volumetric Fog, Unreal Engine 4.27 documentation. | [docs](https://docs.unrealengine.com/4.27/BuildingWorlds/FogEffects/VolumetricFog/) | Froxel history reuse for lighting | full |

## D. Detail and adaptive grids

| Key | Reference | DOI / link | Why we cite it | Checked |
|---|---|---|---|---|
| Kim08 | T. Kim, N. Thürey, D. James, M. Gross. Wavelet Turbulence for Fluid Simulation. ACM TOG 27(3), 2008. | [10.1145/1360612.1360649](https://doi.org/10.1145/1360612.1360649) | Detail synthesis over a coarse sim | meta |
| Bridson07 | R. Bridson, J. Hourihan, M. Nordenstam. Curl-Noise for Procedural Fluid Flow. ACM TOG 26(3), 2007. | [10.1145/1276377.1276435](https://doi.org/10.1145/1276377.1276435) | Seeding newly visible froxels | meta |
| Rasmussen03 | N. Rasmussen, D. Q. Nguyen, W. Geiger, R. Fedkiw. Smoke Simulation for Large Scale Phenomena. ACM TOG 22(3), 2003. | [10.1145/882262.882335](https://doi.org/10.1145/882262.882335) | Coarse sim plus detail for large smoke | meta |
| Losasso04 | F. Losasso, F. Gibou, R. Fedkiw. Simulating Water and Smoke with an Octree Data Structure. ACM TOG 23(3), 2004. | [10.1145/1015706.1015745](https://doi.org/10.1145/1015706.1015745) | Adaptive grids, contrast to ours | meta |
| Setaluri14 | R. Setaluri, M. Aanjaneya, S. Bauer, E. Sifakis. SPGrid. ACM TOG 33(6), 2014. | [10.1145/2661229.2661269](https://doi.org/10.1145/2661229.2661269) | Sparse adaptive smoke, contrast to ours | meta |
| Raateland22 | W. Raateland et al. DCGrid: An Adaptive Grid Structure for Memory-Constrained Fluid Simulation on the GPU. PACMCGIT 5(1), 2022. | [10.1145/3522608](https://doi.org/10.1145/3522608) | Memory-budget adaptivity; camera proximity only an optional priority | full |

## E. Numerical methods in the plan

| Key | Reference | DOI / link | Why we cite it | Checked |
|---|---|---|---|---|
| Kim07 | B. Kim, Y. Liu, I. Llamas, J. Rossignac. Advections with Significantly Reduced Dissipation and Diffusion (BFECC). IEEE TVCG 13(1), 2007. | [10.1109/TVCG.2007.3](https://doi.org/10.1109/TVCG.2007.3) | Less blur in the transfer | meta |
| Selle08 | A. Selle, R. Fedkiw, B. Kim, Y. Liu, J. Rossignac. An Unconditionally Stable MacCormack Method. J. Sci. Comput. 35, 2008. | [10.1007/s10915-007-9166-4](https://doi.org/10.1007/s10915-007-9166-4) | Less blur in the transfer | meta |

## E2. Particles and turbulence energy (variant P, extensions T2/T3)

| Key | Reference | DOI / link | Why we cite it | Checked |
|---|---|---|---|---|
| Zhu05 | Y. Zhu, R. Bridson. Animating Sand as a Fluid. ACM TOG 24(3), 2005. | [10.1145/1073204.1073298](https://doi.org/10.1145/1073204.1073298) | PIC/FLIP in graphics: particles carry state, grid gives pressure (variant P) | meta |
| Selle05 | A. Selle, N. Rasmussen, R. Fedkiw. A Vortex Particle Method for Smoke, Water and Explosions. ACM TOG 24(3), 2005. | [10.1145/1073204.1073282](https://doi.org/10.1145/1073204.1073282) | Particles add detail to a grid sim (variant P) | meta |
| Narain08 | R. Narain, J. Sewall, M. Carlson, M. C. Lin. Fast Animation of Turbulence Using Energy Transport and Procedural Synthesis. ACM TOG 27(5), 2008. | [10.1145/1409060.1409119](https://doi.org/10.1145/1409060.1409119) | Turbulent energy on the coarse grid, persistent synthesized detail; mechanism behind T2 and T3. Not view-dependent. | full |
| Pfaff10 | T. Pfaff, N. Thuerey, J. Cohen, S. Tariq, M. Gross. Scalable Fluid Simulation Using Anisotropic Turbulence Particles. ACM TOG 29(6), 2010. | [10.1145/1882261.1866196](https://doi.org/10.1145/1882261.1866196) | Very coarse grid plus detail particles; turbulence only on rendered particles. Closest to variant P; says nothing about camera motion or off-screen detail. | full |
| Chentanez14 | N. Chentanez, M. Müller. Mass-Conserving Eulerian Liquid Simulation. IEEE TVCG 20(1), 2014. | [10.1109/TVCG.2013.19](https://doi.org/10.1109/TVCG.2013.19) | Mass correction idea behind T1 | meta |

## F. Evaluation

| Key | Reference | DOI / link | Why we cite it | Checked |
|---|---|---|---|---|
| Andersson20 | P. Andersson, J. Nilsson, T. Akenine-Möller, M. Oskarsson, K. Åström, M. D. Fairchild. FLIP: A Difference Evaluator for Alternating Images. PACMCGIT 3(2), 2020. | [10.1145/3406183](https://doi.org/10.1145/3406183) | Image error metric | meta |

## Search log

Kept so the report can state how the novelty claim was checked.

| Date | Source | Query or method | Result |
|---|---|---|---|
| 2026-10-29 | Web search, patents, GitHub | frustum grid smoke, froxel simulation, up-res | First version of research.md |
| 2026-11-01 | Semantic Scholar API | citations of Gao13 | 21 citing works; none camera-aligned with transferred state |
| 2026-11-01 | Semantic Scholar API | citations of Woo06 | 1 entry (Senkic 2010, Maya vs Blender comparison) |
| 2026-11-01 | Google Scholar | "Realistic smoke simulation using a frustum aligned grid" | Woo06 found; no "cited by" link shown |
| 2026-11-01 | Google Scholar | "frustum-aligned" smoke simulation camera | Woo06, Wronski (fog), ILM-style talk (Lucy, rendering), froxel TAA thesis 2026 (rendering only) |
| 2026-11-01 | Google Scholar | froxel simulation; froxel fluid simulation advection; "camera-aligned grid" fluid OR smoke simulation | No fluid simulation on froxels |
| 2026-11-01 | Google Scholar | view-dependent fluid simulation "moving camera" grid detail | Nothing relevant (reconstruction / NeRF papers) |
| 2026-11-01 | Crossref | view-dependent smoke simulation camera adaptive grid; perspective grid smoke simulation; froxel; temporal reprojection volumetric simulation smoke | Found Bunlutangtum11; otherwise nothing new |
| 2026-11-01 | GitHub code search | "froxel" + advection / divergence / navier / pressure jacobi / advect / "stable fluids" | ~40 hits per query, almost all fog renderers; one overlap: ByroRedux |
| 2026-11-01 | Web search | foveated / temporal fluid VR; moving and translating grids | Wang24/25/26, Shah04, Patel05, Zhu13 |
| 2026-11-01 | Web search | off-screen detail cache / restore; mass-conserving residual transfer; resolution-change detail synthesis; FLIP on frustum or view-dependent grids | Narain08, Pfaff10, Chentanez14; no transfer of a detail layer between frusta, no FLIP on a frustum grid found |
| open | Shadertoy | froxel, advect | Blocked by the site's bot check; to do by hand |

## Still open

- Shadertoy search by hand ("froxel", "frustum advect").
- Full texts of Gao13 and Wang26 (via the university network) before quoting details.
- Every entry marked **meta**: read before citing anything specific from it.
