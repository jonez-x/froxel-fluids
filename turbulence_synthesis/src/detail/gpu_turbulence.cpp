#include "detail/gpu_turbulence.hpp"

#include "froxel_link/gpu_froxel_link.hpp"

#include <ff_turbulence_shaders.hpp>

#include <cmath>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>

namespace ff::detail {

namespace {

constexpr int kMaxLayers = 8;
constexpr uint32_t kWorkgroup = 4;

// C++ mirrors of the WGSL structs in shaders/turbulence.wgsl.
struct LayerGpu {
    float freqAmp[4];
    int32_t cell[3][4];
    float frac[3][4];
};
struct ParamsGpu {
    froxel_link::FroxelUniform froxel;  // camera + froxel grid, W2 layout (160 bytes)
    float world[4];
    uint32_t worldN[4];
    float synth[4];
    uint32_t obstacle[4];
    LayerGpu layer[kMaxLayers];
};
static_assert(sizeof(LayerGpu) == 112);
static_assert(sizeof(froxel_link::FroxelUniform) == 160);
static_assert(offsetof(ParamsGpu, world) == 160);
static_assert(sizeof(ParamsGpu) == 160 + 4 * 16 + kMaxLayers * 112);

WGPUBindGroupLayoutEntry bufferEntry(uint32_t binding, WGPUBufferBindingType type) {
    WGPUBindGroupLayoutEntry e{};
    e.binding = binding;
    e.visibility = WGPUShaderStage_Compute;
    e.buffer.type = type;
    return e;
}

}  // namespace

// ---------------------------------------------------------------- GpuTurbulence

struct GpuTurbulence::Impl {
    const gpu::GpuContext& ctx;
    DetailSettings settings;
    const Obstacles* obstacles;
    TurbulenceSynthesis reference;
    const WorldField& world;
    uint32_t obstacleCount = 0;
    gpu::GpuBuffer perm, vort;
    std::unique_ptr<gpu::GpuBuffer> shapes, grids;
    gpu::BindGroupLayout layout;
    gpu::PipelineLayout pipelineLayout;
    gpu::ComputePipeline pipeline;

    Impl(const gpu::GpuContext& c, const WorldField& w, DetailSettings s, const Obstacles* obs)
        : ctx(c), settings(s), obstacles(obs), reference(w, s, obs), world(w),
          perm(c, 512 * 4, WGPUBufferUsage_Storage, "turbulence perm"),
          vort(c, uint64_t(w.size()) * w.size() * w.size() * 4, WGPUBufferUsage_Storage, "turbulence vorticity") {
        if (s.maxLayers > kMaxLayers) throw std::invalid_argument("GpuTurbulence: maxLayers > 8");
        uint32_t p[512];
        const auto& table = reference.noise().permutation();
        for (int i = 0; i < 512; ++i) p[i] = table[size_t(i)];
        perm.write(c, p, sizeof(p));
        const std::vector<double>& vm = w.vorticityMagnitude();
        std::vector<float> v(vm.begin(), vm.end());
        vort.write(c, v.data(), v.size() * 4);

        // Obstacles in the GPU layout (spec 009); throws for shapes the GPU cannot represent.
        ObstacleGpuData data = obs ? obs->gpuData() : ObstacleGpuData{};
        obstacleCount = uint32_t(data.shapes.size());
        if (data.shapes.empty()) data.shapes.assign(1, ObstacleShapeGpu{});  // bindings need non-empty buffers
        if (data.grids.empty()) data.grids.assign(4, 0.0f);
        const uint64_t shapeBytes = data.shapes.size() * sizeof(ObstacleShapeGpu);
        shapes = std::make_unique<gpu::GpuBuffer>(c, shapeBytes, WGPUBufferUsage_Storage, "turbulence shapes");
        shapes->write(c, data.shapes.data(), shapeBytes);
        grids = std::make_unique<gpu::GpuBuffer>(c, data.grids.size() * 4, WGPUBufferUsage_Storage, "turbulence grids");
        grids->write(c, data.grids.data(), data.grids.size() * 4);

        // turbulence.wgsl uses the W2 froxel mapping (FroxelUniform, froxel_center), prepended here.
        const std::string source = froxel_link::withFroxelGeometry(turbulence_shaders::turbulence_wgsl);
        WGPUShaderSourceWGSL src{};
        src.chain.sType = WGPUSType_ShaderSourceWGSL;
        src.code = gpu::sv(source);
        WGPUShaderModuleDescriptor md{};
        md.nextInChain = &src.chain;
        md.label = gpu::sv("turbulence");
        gpu::ShaderModule module(wgpuDeviceCreateShaderModule(c.device(), &md));

        const WGPUBindGroupLayoutEntry entries[7] = {
            bufferEntry(0, WGPUBufferBindingType_Uniform), bufferEntry(1, WGPUBufferBindingType_ReadOnlyStorage),
            bufferEntry(2, WGPUBufferBindingType_ReadOnlyStorage), bufferEntry(3, WGPUBufferBindingType_ReadOnlyStorage),
            bufferEntry(4, WGPUBufferBindingType_Storage), bufferEntry(5, WGPUBufferBindingType_ReadOnlyStorage),
            bufferEntry(6, WGPUBufferBindingType_ReadOnlyStorage)};
        WGPUBindGroupLayoutDescriptor bld{};
        bld.label = gpu::sv("turbulence");
        bld.entryCount = 7;
        bld.entries = entries;
        layout.reset(wgpuDeviceCreateBindGroupLayout(c.device(), &bld));
        WGPUBindGroupLayout layouts[] = {layout.get()};
        WGPUPipelineLayoutDescriptor pld{};
        pld.bindGroupLayoutCount = 1;
        pld.bindGroupLayouts = layouts;
        pipelineLayout.reset(wgpuDeviceCreatePipelineLayout(c.device(), &pld));
        WGPUComputePipelineDescriptor cpd{};
        cpd.label = gpu::sv("turbulence");
        cpd.layout = pipelineLayout.get();
        cpd.compute.module = module.get();
        cpd.compute.entryPoint = gpu::sv("main");
        pipeline.reset(wgpuDeviceCreateComputePipeline(c.device(), &cpd));
        if (!pipeline) throw std::runtime_error("cannot create the turbulence pipeline");
    }
};

GpuTurbulence::GpuTurbulence(const gpu::GpuContext& ctx, const WorldField& world, DetailSettings settings,
                             const Obstacles* obstacles)
    : impl_(std::make_unique<Impl>(ctx, world, settings, obstacles)) {}
GpuTurbulence::~GpuTurbulence() = default;
const DetailSettings& GpuTurbulence::settings() const { return impl_->settings; }
const TurbulenceSynthesis& GpuTurbulence::reference() const { return impl_->reference; }

// ---------------------------------------------------------------- FroxelDetail

struct FroxelDetail::Impl {
    const gpu::GpuContext& ctx;
    camera::FroxelGrid grid;
    gpu::GpuBuffer out, params, slices;

    Impl(const gpu::GpuContext& c, const camera::FroxelGrid& g)
        : ctx(c), grid(g),
          out(c, uint64_t(g.nx) * g.ny * g.nz * 16, WGPUBufferUsage_Storage, "froxel detail"),
          params(c, sizeof(ParamsGpu), WGPUBufferUsage_Uniform, "turbulence params"),
          slices(c, uint64_t(g.nz) * 16, WGPUBufferUsage_Storage, "turbulence slices") {}
};

FroxelDetail::FroxelDetail(const gpu::GpuContext& ctx, const camera::FroxelGrid& grid)
    : impl_(std::make_unique<Impl>(ctx, grid)) {}
FroxelDetail::~FroxelDetail() = default;
const gpu::GpuBuffer& FroxelDetail::buffer() const { return impl_->out; }
const camera::FroxelGrid& FroxelDetail::grid() const { return impl_->grid; }

void FroxelDetail::synthesize(const GpuTurbulence& t, const camera::Camera& cam, gpu::GpuTimer* timer) {
    const GpuTurbulence::Impl& T = t.impl();
    const camera::FroxelGrid& g = impl_->grid;
    const WorldField& w = T.world;

    // Slice table: layer count per slice (research R4). Froxel centers come from the W2 uniform.
    std::vector<float> sl(size_t(g.nz) * 4, 0.0f);
    int usedLayers = 0;
    for (uint32_t k = 0; k < g.nz; ++k) {
        const int layers = T.reference.layersForSlice(cam, g, k);
        sl[4 * k + 1] = float(layers);
        usedLayers = std::max(usedLayers, layers);
    }
    impl_->slices.write(impl_->ctx, sl.data(), sl.size() * 4);

    ParamsGpu P{};
    P.froxel = froxel_link::froxelUniform(cam, g);
    P.world[0] = float(w.origin().x), P.world[1] = float(w.origin().y), P.world[2] = float(w.origin().z);
    P.world[3] = float(w.cellSize());
    P.worldN[0] = w.size();
    P.synth[0] = float(T.settings.strength * w.cellSize());
    P.synth[1] = float(T.reference.wallDistance());
    P.synth[2] = float(T.settings.obstacleFade);
    P.obstacle[0] = T.obstacleCount;
    for (int i = 1; i <= std::min(usedLayers, kMaxLayers); ++i) {
        const LayerConstants lc = layerConstants(w.cellSize(), i);
        LayerGpu& L = P.layer[i - 1];
        L.freqAmp[0] = float(lc.frequency), L.freqAmp[1] = float(lc.amplitude), L.freqAmp[2] = float(lc.potentialAmp);
        for (int c = 0; c < 3; ++c)
            for (int a = 0; a < 3; ++a) {
                const double fl = std::floor(lc.offset[c][a]);
                L.cell[c][a] = int32_t(fl);
                L.frac[c][a] = float(lc.offset[c][a] - fl);
            }
    }
    impl_->params.write(impl_->ctx, &P, sizeof(P));

    WGPUBindGroupEntry be[7]{};
    const gpu::GpuBuffer* bufs[7] = {&impl_->params, &T.perm, &T.vort, &impl_->slices, &impl_->out, T.shapes.get(), T.grids.get()};
    for (uint32_t i = 0; i < 7; ++i) {
        be[i].binding = i;
        be[i].buffer = bufs[i]->get();
        be[i].size = bufs[i]->size();
    }
    WGPUBindGroupDescriptor bgd{};
    bgd.layout = T.layout.get();
    bgd.entryCount = 7;
    bgd.entries = be;
    gpu::BindGroup group(wgpuDeviceCreateBindGroup(impl_->ctx.device(), &bgd));

    WGPUCommandEncoderDescriptor ed{};
    gpu::CommandEncoder enc(wgpuDeviceCreateCommandEncoder(impl_->ctx.device(), &ed));
    WGPUComputePassDescriptor pd{};
    pd.label = gpu::sv("turbulence");
    pd.timestampWrites = timer ? timer->beginPass("turbulence") : nullptr;
    gpu::ComputePassEncoder pass(wgpuCommandEncoderBeginComputePass(enc.get(), &pd));
    wgpuComputePassEncoderSetPipeline(pass.get(), T.pipeline.get());
    wgpuComputePassEncoderSetBindGroup(pass.get(), 0, group.get(), 0, nullptr);
    auto groups = [](uint32_t n) { return (n + kWorkgroup - 1) / kWorkgroup; };
    wgpuComputePassEncoderDispatchWorkgroups(pass.get(), groups(g.nx), groups(g.ny), groups(g.nz));
    wgpuComputePassEncoderEnd(pass.get());
    WGPUCommandBufferDescriptor cd{};
    gpu::CommandBuffer cmd(wgpuCommandEncoderFinish(enc.get(), &cd));
    WGPUCommandBuffer cmds[] = {cmd.get()};
    wgpuQueueSubmit(impl_->ctx.queue(), 1, cmds);
}

FroxelDetailField FroxelDetail::readBack(const camera::Camera& cam) const {
    const camera::FroxelGrid& g = impl_->grid;
    const size_t n = size_t(g.nx) * g.ny * g.nz;
    const std::vector<float> raw = impl_->out.readAs<float>(impl_->ctx, n * 4);
    std::vector<glm::vec4> data(n);
    std::memcpy(data.data(), raw.data(), n * sizeof(glm::vec4));
    return FroxelDetailField(cam, g, std::move(data));
}

}  // namespace ff::detail
