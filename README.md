# Froxel Fluids

Hybrid smoke simulation: a coarse world grid (Stable Fluids) plus a camera-aligned froxel grid for fine detail. Research focus: transporting the simulated detail state between camera frusta when the camera moves.

Computer Graphics Project, University of Tübingen, Group G04.

Research and tech stack: see [planning/](planning/).

Status: research and planning done (2026-11-01). CPU prototype in [rust-cpu-prototype/](rust-cpu-prototype/).

W2 froxel grid module (Andreas): [code and integration guide](froxel_grid/README.md).

The module provides camera/grid geometry, geometric history and GPU density sampling.
It requires the shared C++/WebGPU `ff_gpu` target and GLM; that common C++ infrastructure
is not included in this branch yet. It is a library for integration, not a standalone
application or an extension of the Rust prototype.
