#include "gpu/froxel_batch.hpp"
#include "gpu/froxel_mapping.hpp"

#include <ff_froxel_shaders.hpp>

#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace ff::gpu {
namespace {
constexpr uint64_t worldOffset = 256;

struct alignas(16) WorldUniform {
    float origin[3];
    float cellSize;
};
static_assert(sizeof(WorldUniform) == 16 && offsetof(WorldUniform, cellSize) == 12);
static_assert(sizeof(camera::FroxelUniform) == 160);

struct Snapshot {
    camera::FroxelUniform camera;
    WorldUniform world;
};
}  // namespace

FroxelDensityBatch::FroxelDensityBatch(const GpuContext& ctx) : owner_(ctx.device()) {
    WGPULimits limits{};
    if (wgpuDeviceGetLimits(ctx.device(), &limits) != WGPUStatus_Success)
        throw std::runtime_error("could not query froxel batch device limits");
    if (!limits.minUniformBufferOffsetAlignment || 256 % limits.minUniformBufferOffsetAlignment != 0 ||
        limits.maxUniformBufferBindingSize < sizeof(camera::FroxelUniform) ||
        limits.maxBufferSize < uniformBytesPerJob)
        throw std::runtime_error("device cannot use the froxel batch uniform layout");
    maxBufferSize_ = limits.maxBufferSize;

    const std::string code = withFroxelMapping(ff::froxel_shaders::froxel_density_wgsl);
    WGPUShaderSourceWGSL source{};
    source.chain.sType = WGPUSType_ShaderSourceWGSL;
    source.code = sv(code);
    WGPUShaderModuleDescriptor md{};
    md.nextInChain = &source.chain;
    md.label = sv("froxel density batch");
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
    bld.label = sv("froxel density batch");
    bld.entryCount = 4;
    bld.entries = entries;
    layout_.reset(wgpuDeviceCreateBindGroupLayout(ctx.device(), &bld));
    const WGPUBindGroupLayout layouts[] = {layout_.get()};
    WGPUPipelineLayoutDescriptor pld{};
    pld.bindGroupLayoutCount = 1;
    pld.bindGroupLayouts = layouts;
    pipelineLayout_.reset(wgpuDeviceCreatePipelineLayout(ctx.device(), &pld));
    WGPUComputePipelineDescriptor cpd{};
    cpd.label = sv("froxel density batch");
    cpd.layout = pipelineLayout_.get();
    cpd.compute.module = module.get();
    cpd.compute.entryPoint = sv("main");
    pipeline_.reset(wgpuDeviceCreateComputePipeline(ctx.device(), &cpd));
    if (!module || !layout_ || !pipelineLayout_ || !pipeline_)
        throw std::runtime_error("could not create froxel density batch pipeline");
}

void FroxelDensityBatch::encode(const GpuContext& ctx, WGPUCommandEncoder encoder,
                                std::span<const FroxelDensityJob> jobs, GpuTimer* timer) {
    if (ctx.device() != owner_) throw std::invalid_argument("froxel batch belongs to another GPU context");
    if (!pipeline_ || !layout_) throw std::invalid_argument("froxel batch is empty or moved from");
    if (!encoder) throw std::invalid_argument("froxel batch needs a command encoder");
    if (jobs.size() > maxBufferSize_ / uniformBytesPerJob ||
        jobs.size() > std::numeric_limits<std::size_t>::max() / uniformBytesPerJob)
        throw std::invalid_argument("froxel batch exceeds maximum uniform buffer size");
    if (jobs.empty()) return;

    std::unordered_set<WGPUTexture> inputs, outputs;
    std::vector<Snapshot> snapshots;
    snapshots.reserve(jobs.size());
    for (const auto& job : jobs) {
        if (!job.world || !job.output) throw std::invalid_argument("froxel batch has a null volume");
        job.world->validate(ctx, WGPUTextureUsage_TextureBinding);
        job.output->validate(ctx, WGPUTextureUsage_StorageBinding);
        const auto cameraData = camera::makeUniform(job.camera, job.grid);
        job.geometry.validate(job.world->size());
        const auto size = job.output->size();
        if (size.x != job.grid.nx || size.y != job.grid.ny || size.z != job.grid.nz)
            throw std::invalid_argument("froxel batch output size must match camera grid");
        inputs.insert(job.world->texture());
        if (!outputs.insert(job.output->texture()).second)
            throw std::invalid_argument("froxel batch outputs must not alias each other");
        Snapshot snapshot{cameraData,
            {{job.geometry.origin.x, job.geometry.origin.y, job.geometry.origin.z}, job.geometry.cellSize}};
        snapshots.push_back(snapshot);
    }
    for (auto output : outputs)
        if (inputs.contains(output))
            throw std::invalid_argument("froxel batch output must not alias any world input");

    // Each encode owns fresh immutable parameters. QueueWriteBuffer on reused uniforms
    // would overwrite earlier cameras when several encodes share one later submission.
    const uint64_t bytes = uint64_t(jobs.size()) * uniformBytesPerJob;
    WGPUBufferDescriptor bd{};
    bd.label = sv("froxel batch parameter snapshots");
    bd.size = bytes;
    bd.usage = WGPUBufferUsage_Uniform;
    bd.mappedAtCreation = true;
    Buffer buffer(wgpuDeviceCreateBuffer(ctx.device(), &bd));
    if (!buffer) throw std::runtime_error("could not allocate froxel batch parameters");
    auto* mapped = static_cast<std::byte*>(wgpuBufferGetMappedRange(buffer.get(), 0, bytes));
    if (!mapped) throw std::runtime_error("could not map froxel batch parameters");
    std::memset(mapped, 0, std::size_t(bytes));
    for (std::size_t i = 0; i < snapshots.size(); ++i) {
        auto* region = mapped + i * uniformBytesPerJob;
        std::memcpy(region, &snapshots[i].camera, sizeof(snapshots[i].camera));
        std::memcpy(region + worldOffset, &snapshots[i].world, sizeof(snapshots[i].world));
    }
    wgpuBufferUnmap(buffer.get());

    // Build every binding before recording the first pass. Command encoding retains
    // the bound resources, so these local references may be released on return.
    std::vector<BindGroup> groups;
    groups.reserve(jobs.size());
    for (std::size_t i = 0; i < jobs.size(); ++i) {
        const uint64_t base = uint64_t(i) * uniformBytesPerJob;
        WGPUBindGroupEntry entries[4]{};
        for (uint32_t binding = 0; binding < 4; ++binding) entries[binding].binding = binding;
        entries[0].buffer = buffer.get();
        entries[0].offset = base;
        entries[0].size = sizeof(camera::FroxelUniform);
        entries[1].buffer = buffer.get();
        entries[1].offset = base + worldOffset;
        entries[1].size = sizeof(WorldUniform);
        entries[2].textureView = jobs[i].world->view();
        entries[3].textureView = jobs[i].output->view();
        WGPUBindGroupDescriptor bgd{};
        bgd.label = sv("froxel batch camera");
        bgd.layout = layout_.get();
        bgd.entryCount = 4;
        bgd.entries = entries;
        groups.emplace_back(wgpuDeviceCreateBindGroup(ctx.device(), &bgd));
        if (!groups.back()) throw std::runtime_error("could not create froxel batch binding");
    }
    for (std::size_t i = 0; i < jobs.size(); ++i) {
        WGPUComputePassDescriptor pd{};
        pd.label = sv("froxel density batch camera");
        if (timer) pd.timestampWrites = timer->beginPass("froxel_density_batch");
        ComputePassEncoder pass(wgpuCommandEncoderBeginComputePass(encoder, &pd));
        wgpuComputePassEncoderSetPipeline(pass.get(), pipeline_.get());
        wgpuComputePassEncoderSetBindGroup(pass.get(), 0, groups[i].get(), 0, nullptr);
        const auto& grid = jobs[i].grid;
        wgpuComputePassEncoderDispatchWorkgroups(pass.get(),
            (grid.nx + 3) / 4, (grid.ny + 3) / 4, (grid.nz + 3) / 4);
        wgpuComputePassEncoderEnd(pass.get());
    }
}

void FroxelDensityBatch::dispatch(const GpuContext& ctx, std::span<const FroxelDensityJob> jobs,
                                  GpuTimer* timer) {
    if (ctx.device() != owner_) throw std::invalid_argument("froxel batch belongs to another GPU context");
    WGPUCommandEncoderDescriptor ed{};
    ed.label = sv("froxel density batch");
    CommandEncoder encoder(wgpuDeviceCreateCommandEncoder(ctx.device(), &ed));
    if (!encoder) throw std::runtime_error("could not create froxel batch encoder");
    encode(ctx, encoder.get(), jobs, timer);
    WGPUCommandBufferDescriptor cd{};
    CommandBuffer command(wgpuCommandEncoderFinish(encoder.get(), &cd));
    if (!command) throw std::runtime_error("could not finish froxel batch encoder");
    const WGPUCommandBuffer commands[] = {command.get()};
    wgpuQueueSubmit(ctx.queue(), 1, commands);
}

}  // namespace ff::gpu
