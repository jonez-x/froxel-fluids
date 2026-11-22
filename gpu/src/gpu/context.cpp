#include "gpu/context.hpp"

#include <webgpu/wgpu.h>

#include <cstdio>

namespace ff::gpu {

struct GpuContext::Impl {
    // Callback data must outlive destruction of the queue, device, adapter and instance.
    std::vector<std::string> errors;
    Instance instance;
    Adapter adapter;
    Device device;
    Queue queue;
    AdapterInfo info;
    bool timestamps = false;
};

namespace {

const char* backendName(WGPUBackendType t) {
    switch (t) {
        case WGPUBackendType_Vulkan: return "Vulkan";
        case WGPUBackendType_Metal: return "Metal";
        case WGPUBackendType_D3D12: return "D3D12";
        case WGPUBackendType_D3D11: return "D3D11";
        case WGPUBackendType_OpenGL: return "OpenGL";
        case WGPUBackendType_OpenGLES: return "OpenGLES";
        case WGPUBackendType_WebGPU: return "WebGPU";
        case WGPUBackendType_Null: return "Null";
        default: return "unknown";
    }
}

const char* adapterTypeName(WGPUAdapterType t) {
    switch (t) {
        case WGPUAdapterType_DiscreteGPU: return "discrete GPU";
        case WGPUAdapterType_IntegratedGPU: return "integrated GPU";
        case WGPUAdapterType_CPU: return "CPU";
        default: return "unknown";
    }
}

struct AdapterRequest {
    WGPUAdapter adapter = nullptr;
    bool done = false;
    std::string message;
};

struct DeviceRequest {
    WGPUDevice device = nullptr;
    bool done = false;
    std::string message;
};

void onAdapter(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* ud1, void*) {
    auto* r = static_cast<AdapterRequest*>(ud1);
    if (status == WGPURequestAdapterStatus_Success) r->adapter = adapter;
    r->message = str(message);
    r->done = true;
}

void onDevice(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* ud1, void*) {
    auto* r = static_cast<DeviceRequest*>(ud1);
    if (status == WGPURequestDeviceStatus_Success) r->device = device;
    r->message = str(message);
    r->done = true;
}

void onUncapturedError(WGPUDevice const*, WGPUErrorType, WGPUStringView message, void* ud1, void*) {
    const std::string m = str(message);
    std::fprintf(stderr, "webgpu error: %s\n", m.c_str());  // print at once: a later submit may abort
    static_cast<std::vector<std::string>*>(ud1)->push_back(m);
}

void onDeviceLost(WGPUDevice const*, WGPUDeviceLostReason reason, WGPUStringView message, void* ud1, void*) {
    // Destroying the device at shutdown is a normal lost event;  only record real losses.
    if (reason == WGPUDeviceLostReason_Destroyed || reason == WGPUDeviceLostReason_InstanceDropped) return;
    static_cast<std::vector<std::string>*>(ud1)->push_back("device lost: " + str(message));
}

}  // namespace

GpuContext::GpuContext(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
GpuContext::GpuContext(GpuContext&&) noexcept = default;
GpuContext& GpuContext::operator=(GpuContext&&) noexcept = default;
GpuContext::~GpuContext() = default;

GpuContext GpuContext::create() {
    auto impl = std::make_unique<Impl>();
    impl->instance.reset(wgpuCreateInstance(nullptr));
    if (!impl->instance) throw NoAdapterError("no suitable GPU adapter found (instance creation failed)");
    WGPUInstance inst = impl->instance.get();

    WGPURequestAdapterOptions opts{};
    opts.powerPreference = WGPUPowerPreference_HighPerformance;
    AdapterRequest ar;
    WGPURequestAdapterCallbackInfo acb{};
    acb.mode = WGPUCallbackMode_AllowProcessEvents;
    acb.callback = onAdapter;
    acb.userdata1 = &ar;
    wgpuInstanceRequestAdapter(inst, &opts, acb);
    while (!ar.done) wgpuInstanceProcessEvents(inst);
    if (!ar.adapter) throw NoAdapterError("no suitable GPU adapter found" + (ar.message.empty() ? "" : ": " + ar.message));
    impl->adapter.reset(ar.adapter);

    WGPUAdapterInfo adapterInfo{};
    if (wgpuAdapterGetInfo(impl->adapter.get(), &adapterInfo) == WGPUStatus_Success) {
        impl->info.name = str(adapterInfo.device);
        if (impl->info.name.empty()) impl->info.name = str(adapterInfo.description);
        impl->info.vendor = str(adapterInfo.vendor);
        if (impl->info.vendor.empty()) impl->info.vendor = "0x" + std::to_string(adapterInfo.vendorID);
        impl->info.backend = backendName(adapterInfo.backendType);
        impl->info.type = adapterTypeName(adapterInfo.adapterType);
        wgpuAdapterInfoFreeMembers(adapterInfo);
    }

    std::vector<WGPUFeatureName> features;
    if (wgpuAdapterHasFeature(impl->adapter.get(), WGPUFeatureName_TimestampQuery)) {
        features.push_back(WGPUFeatureName_TimestampQuery);
    }

    WGPUDeviceDescriptor dd{};
    dd.label = sv("froxel-fluids device");
    dd.requiredFeatureCount = features.size();
    dd.requiredFeatures = features.data();
    dd.uncapturedErrorCallbackInfo.callback = onUncapturedError;
    dd.uncapturedErrorCallbackInfo.userdata1 = &impl->errors;
    dd.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    dd.deviceLostCallbackInfo.callback = onDeviceLost;
    dd.deviceLostCallbackInfo.userdata1 = &impl->errors;

    DeviceRequest dr;
    WGPURequestDeviceCallbackInfo dcb{};
    dcb.mode = WGPUCallbackMode_AllowProcessEvents;
    dcb.callback = onDevice;
    dcb.userdata1 = &dr;
    wgpuAdapterRequestDevice(impl->adapter.get(), &dd, dcb);
    while (!dr.done) wgpuInstanceProcessEvents(inst);
    if (!dr.device) throw NoAdapterError("no suitable GPU device found" + (dr.message.empty() ? "" : ": " + dr.message));
    impl->device.reset(dr.device);
    impl->queue.reset(wgpuDeviceGetQueue(dr.device));
    impl->timestamps = !features.empty();
    return GpuContext(std::move(impl));
}

const AdapterInfo& GpuContext::info() const { return impl_->info; }
bool GpuContext::hasTimestamps() const { return impl_->timestamps; }
const std::vector<std::string>& GpuContext::errors() const { return impl_->errors; }
WGPUInstance GpuContext::instance() const { return impl_->instance.get(); }
WGPUDevice GpuContext::device() const { return impl_->device.get(); }
WGPUQueue GpuContext::queue() const { return impl_->queue.get(); }

void GpuContext::waitUntil(const std::function<bool()>& done) const {
    while (!done()) {
        wgpuDevicePoll(impl_->device.get(), true, nullptr);
        wgpuInstanceProcessEvents(impl_->instance.get());
    }
}

}  // namespace   ff::gpu
