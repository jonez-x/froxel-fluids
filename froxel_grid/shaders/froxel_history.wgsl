// Binding-free geometric history helpers, after froxel_mapping.wgsl.
// No velocity backtrace, scalar resampling, history blending, or seeding is performed here.
struct HistoryUniform {
    current: FroxelUniform,
    previous: FroxelUniform,
    hasHistory: u32,
    identityMapping: u32,
    pad0: u32,
    pad1: u32,
}

struct HistoryCoord {
    previousUVW: vec3<f32>,
    flags: u32,  // Inside=1, Sampleable=2, global HistoryPresent=4.
}

struct HistoryStencil {
    lo: vec3<u32>,
    hi: vec3<u32>,
    weights: vec3<f32>,
    valid: bool,
}

fn history_empty(params: HistoryUniform) -> HistoryCoord {
    return HistoryCoord(vec3<f32>(0.0), select(0u, 4u, params.hasHistory != 0u));
}

// Strict center support, not just geometric inclusion. No epsilon or fringe clamping.
fn history_sampleable(previous: FroxelUniform, uvw: vec3<f32>) -> bool {
    if (!froxel_valid_parameters(previous) || !froxel_finite3(uvw)) { return false; }
    let last = vec3<f32>(f32(previous.nx), f32(previous.ny), f32(previous.nz)) - vec3<f32>(0.5);
    return all(uvw >= vec3<f32>(0.5)) && all(uvw <= last);
}

fn history_stencil(previous: FroxelUniform, uvw: vec3<f32>) -> HistoryStencil {
    var result = HistoryStencil(vec3<u32>(0u), vec3<u32>(0u), vec3<f32>(0.0), false);
    if (!history_sampleable(previous, uvw)) { return result; }
    let q = uvw - vec3<f32>(0.5);
    result.lo = vec3<u32>(floor(q));
    result.hi = min(result.lo + vec3<u32>(1u), vec3<u32>(previous.nx, previous.ny, previous.nz) - vec3<u32>(1u));
    result.weights = q - vec3<f32>(result.lo);
    result.valid = true;
    return result;
}

fn history_from_coord(params: HistoryUniform, coord: FroxelCoord) -> HistoryCoord {
    var result = history_empty(params);
    if (params.hasHistory == 0u || !froxel_finite3(coord.uvw)) { return result; }
    result.previousUVW = coord.uvw;
    if (!coord.inside) { return result; }
    result.flags = result.flags | 1u;
    let stencil = history_stencil(params.previous, coord.uvw);
    if (stencil.valid) { result.flags = result.flags | 2u; }
    return result;
}

// W6 may supply an already backtraced world point without a preliminary resample.
// Even identical camera snapshots must not bypass this supplied-world mapping.
fn world_to_previous_froxel(params: HistoryUniform, world: vec3<f32>) -> HistoryCoord {
    if (params.hasHistory == 0u || !froxel_finite3(world)) { return history_empty(params); }
    return history_from_coord(params, world_to_froxel(params.previous, world));
}

fn reproject_froxel(params: HistoryUniform, currentUVW: vec3<f32>) -> HistoryCoord {
    if (params.hasHistory == 0u || !froxel_finite3(currentUVW)) { return history_empty(params); }
    if (params.identityMapping != 0u) {
        return history_from_coord(params, FroxelCoord(currentUVW, froxel_inside(params.previous, currentUVW)));
    }
    let world = froxel_to_world_checked(params.current, currentUVW);
    if (!world.valid) { return history_empty(params); }
    return world_to_previous_froxel(params, world.world);
}
