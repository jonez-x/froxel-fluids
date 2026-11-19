#include "gpu/froxel_density.hpp"
#include "gpu/froxel_mapping.hpp"

#include <ff_froxel_shaders.hpp>

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <type_traits>

namespace ff::gpu {
namespace {
struct alignas(16) WorldUniform {
    float origin[3];
    float cellSize;
};
static_assert(std::is_standard_layout_v<WorldUniform> && sizeof(WorldUniform) == 16);
static_assert(alignof(WorldUniform) == 16 && offsetof(WorldUniform, cellSize) == 12);

Buffer uniformBuffer(const GpuContext& ctx, uint64_t bytes, std::string_view label) {
    WGPUBufferDescriptor d{};
    d.label = sv(label);
    d.size = bytes;
    d.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    return Buffer(wgpuDeviceCreateBuffer(ctx.device(), &d));
}
}  // namespace

void WorldGridGeometry::validate(Extent3D extent) const {
    if (!(cellSize > 0) || !std::isfinite(cellSize)) throw std::invalid_argument("invalid world cell spacing");
    const uint32_t axes[] = {extent.x, extent.y, extent.z};
    for (int axis = 0; axis < 3; ++axis) {
        if (!axes[axis] || axes[axis] > 256 || !std::isfinite(origin[axis]) ||
            !std::isfinite(origin[axis] + cellSize * float(axes[axis])))
            throw std::invalid_argument("invalid world grid geometry");
    }
}

FroxelDensitySampler::FroxelDensitySampler(const GpuContext& ctx)
    : owner_(ctx.device()), cameraUniform_(uniformBuffer(ctx, sizeof(camera::FroxelUniform), "froxel camera")),
      worldUniform_(uniformBuffer(ctx, sizeof(WorldUniform), "world geometry")) {
    const std::string code = withFroxelMapping(ff::froxel_shaders::froxel_density_wgsl);
    WGPUShaderSourceWGSL source{};
    source.chain.sType = WGPUSType_ShaderSourceWGSL;
    source.code = sv(code);
    WGPUShaderModuleDescriptor md{};
    md.nextInChain = &source.chain;
    md.label = sv("froxel density");
    ShaderModule module(wgpuDeviceCreateShaderModule(ctx.device(), &md));
    WGPUBindGroupLayoutEntry entries[4]{};
    for (uint32_t i = 0; i < 4; ++i) {
        entries[i].binding = i;
        entries[i].visibility = WGPUShaderStage_Compute;
    }
    entries[0].buffer.type = WGPUBufferBindingType_Uniform;
    entries[0].buffer.minBindingSize = sizeof(camera::FroxelUniform);
    entries[1].buffer.type = WGPUBufferBindingType_Uniform;
    entries[1].buffer.minBindingSize = sizeof(WorldUniform);
    entries[2].texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
    entries[2].texture.viewDimension = WGPUTextureViewDimension_3D;
    entries[3].storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
    entries[3].storageTexture.format = WGPUTextureFormat_R32Float;
    entries[3].storageTexture.viewDimension = WGPUTextureViewDimension_3D;
    WGPUBindGroupLayoutDescriptor bld{};
    bld.entryCount = 4;
    bld.entries = entries;
    layout_.reset(wgpuDeviceCreateBindGroupLayout(ctx.device(), &bld));
    const WGPUBindGroupLayout layouts[] = {layout_.get()};
    WGPUPipelineLayoutDescriptor pld{};
    pld.bindGroupLayoutCount = 1;
    pld.bindGroupLayouts = layouts;
    pipelineLayout_.reset(wgpuDeviceCreatePipelineLayout(ctx.device(), &pld));
    WGPUComputePipelineDescriptor cpd{};
    cpd.label = sv("froxel density");
    cpd.layout = pipelineLayout_.get();
    cpd.compute.module = module.get();
    cpd.compute.entryPoint = sv("main");
    pipeline_.reset(wgpuDeviceCreateComputePipeline(ctx.device(), &cpd));
    if (!module || !layout_ || !pipelineLayout_ || !pipeline_ || !cameraUniform_ || !worldUniform_)
        throw std::runtime_error("could not create froxel density pipeline");
}

void FroxelDensitySampler::dispatch(const GpuContext& ctx, const camera::Camera& cam,
                                   const camera::FroxelGrid& grid, const FloatVolume& world,
                                   const WorldGridGeometry& geometry, FloatVolume& output, GpuTimer* timer) {
    if (ctx.device() != owner_) throw std::invalid_argument("froxel sampler belongs to another GPU context");
    if (!pipeline_ || !cameraUniform_ || !worldUniform_)
        throw std::invalid_argument("froxel sampler is empty or moved from");
    world.validate(ctx, WGPUTextureUsage_TextureBinding);
    output.validate(ctx, WGPUTextureUsage_StorageBinding);
    const auto cameraData = camera::makeUniform(cam, grid);
    geometry.validate(world.size());
    const auto size = output.size();
    if (size.x != grid.nx || size.y != grid.ny || size.z != grid.nz)
        throw std::invalid_argument("froxel output size must match camera grid");
    if (world.texture() == output.texture()) throw std::invalid_argument("world and froxel textures must not alias");
    const WorldUniform worldData{{geometry.origin.x, geometry.origin.y, geometry.origin.z}, geometry.cellSize};
    wgpuQueueWriteBuffer(ctx.queue(), cameraUniform_.get(), 0, &cameraData, sizeof(cameraData));
    wgpuQueueWriteBuffer(ctx.queue(), worldUniform_.get(), 0, &worldData, sizeof(worldData));
    WGPUBindGroupEntry entries[4]{};
    for (uint32_t i = 0; i < 4; ++i) entries[i].binding = i;
    entries[0].buffer = cameraUniform_.get();
    entries[0].size = sizeof(cameraData);
    entries[1].buffer = worldUniform_.get();
    entries[1].size = sizeof(worldData);
    entries[2].textureView = world.view();
    entries[3].textureView = output.view();
    WGPUBindGroupDescriptor bgd{};
    bgd.layout = layout_.get();
    bgd.entryCount = 4;
    bgd.entries = entries;
    BindGroup group(wgpuDeviceCreateBindGroup(ctx.device(), &bgd));
    WGPUCommandEncoderDescriptor ed{};
    CommandEncoder encoder(wgpuDeviceCreateCommandEncoder(ctx.device(), &ed));
    WGPUComputePassDescriptor pd{};
    pd.label = sv("froxel density");
    if (timer) pd.timestampWrites = timer->beginPass("froxel_density");
    ComputePassEncoder pass(wgpuCommandEncoderBeginComputePass(encoder.get(), &pd));
    wgpuComputePassEncoderSetPipeline(pass.get(), pipeline_.get());
    wgpuComputePassEncoderSetBindGroup(pass.get(), 0, group.get(), 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(pass.get(), (size.x + 3) / 4, (size.y + 3) / 4, (size.z + 3) / 4);
    wgpuComputePassEncoderEnd(pass.get());
    WGPUCommandBufferDescriptor cd{};
    CommandBuffer command(wgpuCommandEncoderFinish(encoder.get(), &cd));
    const WGPUCommandBuffer commands[] = {command.get()};
    wgpuQueueSubmit(ctx.queue(), 1, commands);
}

}  // namespace ff::gpu
