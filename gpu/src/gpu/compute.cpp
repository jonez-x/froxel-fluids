#include "gpu/compute.hpp"

#include <stdexcept>

namespace ff::gpu {

ComputeProgram::ComputeProgram(const GpuContext& ctx, std::string label, std::string_view wgsl,
                               std::vector<BindingKind> layout, std::string_view entryPoint)
    : label_(std::move(label)), kinds_(std::move(layout)) {
    WGPUShaderSourceWGSL src{};
    src.chain.sType = WGPUSType_ShaderSourceWGSL;
    src.code = sv(wgsl);
    WGPUShaderModuleDescriptor md{};
    md.nextInChain = &src.chain;
    md.label = sv(label_);
    ShaderModule module(wgpuDeviceCreateShaderModule(ctx.device(), &md));

    std::vector<WGPUBindGroupLayoutEntry> entries(kinds_.size());
    for (size_t i = 0; i < kinds_.size(); ++i) {
        WGPUBindGroupLayoutEntry& e = entries[i];
        e.binding = uint32_t(i);
        e.visibility = WGPUShaderStage_Compute;
        switch (kinds_[i]) {
            case BindingKind::Uniform: e.buffer.type = WGPUBufferBindingType_Uniform; break;
            case BindingKind::ReadOnlyStorage: e.buffer.type = WGPUBufferBindingType_ReadOnlyStorage; break;
            case BindingKind::Storage: e.buffer.type = WGPUBufferBindingType_Storage; break;
            case BindingKind::Texture3D:
            case BindingKind::Texture2D:
                e.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
                e.texture.viewDimension =
                    kinds_[i] == BindingKind::Texture3D ? WGPUTextureViewDimension_3D : WGPUTextureViewDimension_2D;
                break;
            case BindingKind::StorageTexture3D:
            case BindingKind::StorageTexture2D:
                e.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
                e.storageTexture.format = WGPUTextureFormat_R32Float;
                e.storageTexture.viewDimension =
                    kinds_[i] == BindingKind::StorageTexture3D ? WGPUTextureViewDimension_3D : WGPUTextureViewDimension_2D;
                break;
        }
    }
    WGPUBindGroupLayoutDescriptor bld{};
    bld.label = sv(label_);
    bld.entryCount = entries.size();
    bld.entries = entries.data();
    layout_.reset(wgpuDeviceCreateBindGroupLayout(ctx.device(), &bld));
    const WGPUBindGroupLayout layouts[] = {layout_.get()};
    WGPUPipelineLayoutDescriptor pld{};
    pld.bindGroupLayoutCount = 1;
    pld.bindGroupLayouts = layouts;
    pipelineLayout_.reset(wgpuDeviceCreatePipelineLayout(ctx.device(), &pld));
    WGPUComputePipelineDescriptor cpd{};
    cpd.label = sv(label_);
    cpd.layout = pipelineLayout_.get();
    cpd.compute.module = module.get();
    cpd.compute.entryPoint = sv(entryPoint);
    pipeline_.reset(wgpuDeviceCreateComputePipeline(ctx.device(), &cpd));
    if (!module || !layout_ || !pipelineLayout_ || !pipeline_)
        throw std::runtime_error("cannot create compute program " + label_);
}

void ComputeProgram::encode(const GpuContext& ctx, WGPUCommandEncoder encoder, std::span<const Binding> bindings,
                            Extent3D groups, GpuTimer* timer) const {
    if (bindings.size() != kinds_.size())
        throw std::invalid_argument(label_ + ": expected " + std::to_string(kinds_.size()) + " bindings");
    std::vector<WGPUBindGroupEntry> entries(bindings.size());
    for (size_t i = 0; i < bindings.size(); ++i) {
        entries[i].binding = uint32_t(i);
        const bool texture = kinds_[i] != BindingKind::Uniform && kinds_[i] != BindingKind::ReadOnlyStorage &&
                             kinds_[i] != BindingKind::Storage;
        if (texture) {
            if (!bindings[i].view) throw std::invalid_argument(label_ + ": binding " + std::to_string(i) + " needs a view");
            entries[i].textureView = bindings[i].view;
        } else {
            if (!bindings[i].buffer) throw std::invalid_argument(label_ + ": binding " + std::to_string(i) + " needs a buffer");
            entries[i].buffer = bindings[i].buffer;
            entries[i].size = bindings[i].size;
        }
    }
    WGPUBindGroupDescriptor bgd{};
    bgd.label = sv(label_);
    bgd.layout = layout_.get();
    bgd.entryCount = entries.size();
    bgd.entries = entries.data();
    BindGroup group(wgpuDeviceCreateBindGroup(ctx.device(), &bgd));
    if (!group) throw std::runtime_error(label_ + ": cannot create the bind group");

    WGPUComputePassDescriptor pd{};
    pd.label = sv(label_);
    if (timer) pd.timestampWrites = timer->beginPass(label_);
    ComputePassEncoder pass(wgpuCommandEncoderBeginComputePass(encoder, &pd));
    wgpuComputePassEncoderSetPipeline(pass.get(), pipeline_.get());
    wgpuComputePassEncoderSetBindGroup(pass.get(), 0, group.get(), 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(pass.get(), groups.x, groups.y, groups.z);
    wgpuComputePassEncoderEnd(pass.get());
}

}  // namespace ff::gpu
