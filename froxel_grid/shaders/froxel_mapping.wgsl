// Shared camera geometry. No bindings or entry point: concatenate before a kernel.
// Right-handed +Y-up world, -Z view direction, v increases upward; depth is axial.
struct FroxelUniform {
    view: mat4x4<f32>,
    invView: mat4x4<f32>,
    tanHalfFovY: f32,
    aspect: f32,
    zNear: f32,
    zFar: f32,
    nx: u32,
    ny: u32,
    nz: u32,
    pad: u32,
}

struct FroxelCoord {
    uvw: vec3<f32>,
    inside: bool,
}

struct FroxelWorldPoint {
    world: vec3<f32>,
    valid: bool,
}

fn froxel_finite(value: f32) -> bool {
    return (bitcast<u32>(value) & 0x7f800000u) != 0x7f800000u;
}

fn froxel_finite3(value: vec3<f32>) -> bool {
    return all((bitcast<vec3<u32>>(value) & vec3<u32>(0x7f800000u)) != vec3<u32>(0x7f800000u));
}

fn froxel_valid_parameters(params: FroxelUniform) -> bool {
    let dims = vec3<u32>(params.nx, params.ny, params.nz);
    if (!all(dims > vec3<u32>(0u)) || !all(dims <= vec3<u32>(8388608u))) { return false; }
    if (!froxel_finite(params.tanHalfFovY) || params.tanHalfFovY <= 0.0 ||
        !froxel_finite(params.aspect) || params.aspect <= 0.0 ||
        !froxel_finite(params.zNear) || params.zNear <= 0.0 ||
        !froxel_finite(params.zFar) || params.zFar <= params.zNear) { return false; }
    return froxel_finite(params.zFar / params.zNear) &&
        froxel_finite(params.tanHalfFovY * params.aspect) && params.tanHalfFovY * params.aspect > 0.0;
}

fn froxel_inside(params: FroxelUniform, uvw: vec3<f32>) -> bool {
    return froxel_finite3(uvw) && all(uvw >= vec3<f32>(0.0)) &&
        all(uvw < vec3<f32>(f32(params.nx), f32(params.ny), f32(params.nz)));
}

fn froxel_slice_depth(params: FroxelUniform, k: f32) -> f32 {
    if (k == 0.0) { return params.zNear; }
    if (k == f32(params.nz)) { return params.zFar; }
    return params.zNear * pow(params.zFar / params.zNear, k / f32(params.nz));
}

fn world_to_froxel(params: FroxelUniform, world: vec3<f32>) -> FroxelCoord {
    var result = FroxelCoord(vec3<f32>(0.0), false);
    if (!froxel_valid_parameters(params) || !froxel_finite3(world)) { return result; }
    let view = (params.view * vec4<f32>(world, 1.0)).xyz;
    let depth = -view.z;
    if (!froxel_finite3(view) || depth <= 0.0) { return result; }
    let denominators = vec2<f32>(depth * params.tanHalfFovY * params.aspect, depth * params.tanHalfFovY);
    if (!froxel_finite(denominators.x) || !froxel_finite(denominators.y) ||
        !all(denominators > vec2<f32>(0.0))) { return result; }
    let ndc = view.xy / denominators;
    var slice = f32(params.nz) * log(depth / params.zNear) / log(params.zFar / params.zNear);
    if (depth == params.zNear) { slice = 0.0; }
    if (depth == params.zFar) { slice = f32(params.nz); }
    let uvw = vec3<f32>((ndc * 0.5 + vec2<f32>(0.5)) * vec2<f32>(f32(params.nx), f32(params.ny)),
                       slice);
    if (!froxel_finite3(uvw)) { return result; }
    result.uvw = uvw;
    result.inside = froxel_inside(params, uvw);
    return result;
}

// Finite out-of-grid coordinates may be extrapolated; overflow is reported invalid.
fn froxel_to_world_checked(params: FroxelUniform, uvw: vec3<f32>) -> FroxelWorldPoint {
    var result = FroxelWorldPoint(vec3<f32>(0.0), false);
    if (!froxel_valid_parameters(params) || !froxel_finite3(uvw)) { return result; }
    let depth = froxel_slice_depth(params, uvw.z);
    if (!froxel_finite(depth) || depth <= 0.0) { return result; }
    let ndc = uvw.xy / vec2<f32>(f32(params.nx), f32(params.ny)) * 2.0 - vec2<f32>(1.0);
    let view = vec3<f32>(ndc.x * params.tanHalfFovY * params.aspect * depth,
                         ndc.y * params.tanHalfFovY * depth, -depth);
    if (!froxel_finite3(view)) { return result; }
    let world = (params.invView * vec4<f32>(view, 1.0)).xyz;
    if (!froxel_finite3(world)) { return result; }
    result.world = world;
    result.valid = true;
    return result;
}

fn froxel_to_world(params: FroxelUniform, uvw: vec3<f32>) -> vec3<f32> {
    return froxel_to_world_checked(params, uvw).world;
}

fn froxel_center(params: FroxelUniform, cell: vec3<u32>) -> vec3<f32> {
    return froxel_to_world(params, vec3<f32>(cell) + vec3<f32>(0.5));
}
