// Appended to froxel_mapping.wgsl. Cell-centered world density, scalar R32Float.
struct WorldUniform {
    origin: vec3<f32>,
    cellSize: f32,
}
@group(0) @binding(0) var<uniform> camera: FroxelUniform;
@group(0) @binding(1) var<uniform> world: WorldUniform;
@group(0) @binding(2) var worldDensity: texture_3d<f32>;
@group(0) @binding(3) var outputDensity: texture_storage_3d<r32float, write>;

fn sample_world_density(position: vec3<f32>) -> f32 {
    let dims = textureDimensions(worldDensity);
    // Test physical bounds before normalization: (nextafter(8,+inf) - (-8))
    // can round to 16 in f32 and falsely admit an outside point into a [-8,8] box.
    let upper = world.origin + vec3<f32>(dims) * world.cellSize;
    if (!froxel_finite3(position) || any(position < world.origin) || any(position > upper)) { return 0.0; }
    let g = (position - world.origin) / world.cellSize;
    if (!froxel_finite3(g)) { return 0.0; }
    // Closed physical world box, unlike the half-open froxel index contract.
    let last = vec3<i32>(dims) - vec3<i32>(1);
    let centered = clamp(g - vec3<f32>(0.5), vec3<f32>(0.0), vec3<f32>(last));
    let lo = vec3<i32>(floor(centered));
    let hi = min(lo + vec3<i32>(1), last);
    let t = centered - vec3<f32>(lo);
    let c00 = mix(textureLoad(worldDensity, vec3<i32>(lo.x, lo.y, lo.z), 0).x,
                  textureLoad(worldDensity, vec3<i32>(hi.x, lo.y, lo.z), 0).x, t.x);
    let c10 = mix(textureLoad(worldDensity, vec3<i32>(lo.x, hi.y, lo.z), 0).x,
                  textureLoad(worldDensity, vec3<i32>(hi.x, hi.y, lo.z), 0).x, t.x);
    let c01 = mix(textureLoad(worldDensity, vec3<i32>(lo.x, lo.y, hi.z), 0).x,
                  textureLoad(worldDensity, vec3<i32>(hi.x, lo.y, hi.z), 0).x, t.x);
    let c11 = mix(textureLoad(worldDensity, vec3<i32>(lo.x, hi.y, hi.z), 0).x,
                  textureLoad(worldDensity, vec3<i32>(hi.x, hi.y, hi.z), 0).x, t.x);
    return mix(mix(c00, c10, t.y), mix(c01, c11, t.y), t.z);
}

@compute @workgroup_size(4, 4, 4)
fn main(@builtin(global_invocation_id) id: vec3<u32>) {
    if (any(id >= textureDimensions(outputDensity))) { return; }
    let point = froxel_to_world_checked(camera, vec3<f32>(id) + vec3<f32>(0.5));
    var density = 0.0;
    if (point.valid) { density = sample_world_density(point.world); }
    textureStore(outputDensity, vec3<i32>(id), vec4<f32>(density, 0.0, 0.0, 0.0));
}
