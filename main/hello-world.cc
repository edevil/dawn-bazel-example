#include <cassert>
#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>
#include <iostream>
#include <string>
#include <webgpu/webgpu_cpp.h>

template <typename Enumeration>
auto as_integer(Enumeration const value) -> typename std::underlying_type<Enumeration>::type {
  return static_cast<typename std::underlying_type<Enumeration>::type>(value);
}

wgpu::Adapter requestAdapter(wgpu::Instance instance, wgpu::RequestAdapterOptions const* options) {
  // A simple structure holding the local information shared with the
  // onAdapterRequestEnded callback.
  struct UserData {
    wgpu::Adapter adapter = nullptr;
    bool requestEnded = false;
  };
  UserData userData;

  // Callback called by wgpuInstanceRequestAdapter when the request returns
  // This is a C++ lambda function, but could be any function defined in the
  // global scope. It must be non-capturing (the brackets [] are empty) so
  // that it behaves like a regular C function pointer, which is what
  // wgpuInstanceRequestAdapter expects (WebGPU being a C API). The workaround
  // is to convey what we want to capture through the pUserData pointer,
  // provided as the last argument of wgpuInstanceRequestAdapter and received
  // by the callback as its last argument.
  wgpu::RequestAdapterCallback onAdapterRequestEnded = [](WGPURequestAdapterStatus status,
                                                          WGPUAdapter adapter, char const* message,
                                                          void* pUserData) {
    UserData& userData = *reinterpret_cast<UserData*>(pUserData);
    if ((wgpu::RequestAdapterStatus)status == wgpu::RequestAdapterStatus::Success) {
      userData.adapter = (wgpu::Adapter)adapter;
    } else {
      std::cout << "Could not get WebGPU adapter: " << message << std::endl;
    }
    userData.requestEnded = true;
  };

  // Call to the WebGPU request adapter procedure
  instance.RequestAdapter(options, onAdapterRequestEnded, (void*)&userData);

  // In theory we should wait until onAdapterReady has been called, which
  // could take some time (what the 'await' keyword does in the JavaScript
  // code). In practice, we know that when the wgpuInstanceRequestAdapter()
  // function returns its callback has been called.
  assert(userData.requestEnded);

  return userData.adapter;
}

wgpu::Device requestDevice(wgpu::Adapter adapter, wgpu::DeviceDescriptor const* options) {
  // A simple structure holding the local information shared with the
  // onAdapterRequestEnded callback.
  struct UserData {
    wgpu::Device device = nullptr;
    bool requestEnded = false;
  };
  UserData userData;

  wgpu::RequestDeviceCallback onDeviceRequestEnded =
      [](WGPURequestDeviceStatus status, WGPUDevice device, char const* message, void* pUserData) {
        UserData& userData = *reinterpret_cast<UserData*>(pUserData);
        if ((wgpu::RequestDeviceStatus)status == wgpu::RequestDeviceStatus::Success) {
          userData.device = (wgpu::Device)device;
        } else {
          std::cout << "Could not get WebGPU device: " << message << std::endl;
        }
        userData.requestEnded = true;
      };

  // Call to the WebGPU request device procedure
  adapter.RequestDevice(options, onDeviceRequestEnded, (void*)&userData);

  assert(userData.requestEnded);

  return userData.device;
}

/**
 * An example of how we can inspect the capabilities of the hardware through
 * the adapter object.
 */
void inspectAdapter(wgpu::Adapter adapter) {
  std::vector<wgpu::FeatureName> features;
  size_t featureCount = adapter.EnumerateFeatures(nullptr);
  features.resize(featureCount);
  adapter.EnumerateFeatures(features.data());

  std::cout << "Adapter features:" << std::endl;
  for (auto f : features) {
    std::cout << " - " << as_integer(f) << std::endl;
  }

  wgpu::SupportedLimits limits = {};
  limits.nextInChain = nullptr;
  bool success = adapter.GetLimits(&limits);
  if (success) {
    std::cout << "Adapter limits:" << std::endl;
    std::cout << " - maxTextureDimension1D: " << limits.limits.maxTextureDimension1D << std::endl;
    std::cout << " - maxTextureDimension2D: " << limits.limits.maxTextureDimension2D << std::endl;
    std::cout << " - maxTextureDimension3D: " << limits.limits.maxTextureDimension3D << std::endl;
    std::cout << " - maxTextureArrayLayers: " << limits.limits.maxTextureArrayLayers << std::endl;
    std::cout << " - maxBindGroups: " << limits.limits.maxBindGroups << std::endl;
    std::cout << " - maxDynamicUniformBuffersPerPipelineLayout: "
              << limits.limits.maxDynamicUniformBuffersPerPipelineLayout << std::endl;
    std::cout << " - maxDynamicStorageBuffersPerPipelineLayout: "
              << limits.limits.maxDynamicStorageBuffersPerPipelineLayout << std::endl;
    std::cout << " - maxSampledTexturesPerShaderStage: "
              << limits.limits.maxSampledTexturesPerShaderStage << std::endl;
    std::cout << " - maxSamplersPerShaderStage: " << limits.limits.maxSamplersPerShaderStage
              << std::endl;
    std::cout << " - maxStorageBuffersPerShaderStage: "
              << limits.limits.maxStorageBuffersPerShaderStage << std::endl;
    std::cout << " - maxStorageTexturesPerShaderStage: "
              << limits.limits.maxStorageTexturesPerShaderStage << std::endl;
    std::cout << " - maxUniformBuffersPerShaderStage: "
              << limits.limits.maxUniformBuffersPerShaderStage << std::endl;
    std::cout << " - maxUniformBufferBindingSize: " << limits.limits.maxUniformBufferBindingSize
              << std::endl;
    std::cout << " - maxStorageBufferBindingSize: " << limits.limits.maxStorageBufferBindingSize
              << std::endl;
    std::cout << " - minUniformBufferOffsetAlignment: "
              << limits.limits.minUniformBufferOffsetAlignment << std::endl;
    std::cout << " - minStorageBufferOffsetAlignment: "
              << limits.limits.minStorageBufferOffsetAlignment << std::endl;
    std::cout << " - maxVertexBuffers: " << limits.limits.maxVertexBuffers << std::endl;
    std::cout << " - maxVertexAttributes: " << limits.limits.maxVertexAttributes << std::endl;
    std::cout << " - maxVertexBufferArrayStride: " << limits.limits.maxVertexBufferArrayStride
              << std::endl;
    std::cout << " - maxInterStageShaderComponents: " << limits.limits.maxInterStageShaderComponents
              << std::endl;
    std::cout << " - maxComputeWorkgroupStorageSize: "
              << limits.limits.maxComputeWorkgroupStorageSize << std::endl;
    std::cout << " - maxComputeInvocationsPerWorkgroup: "
              << limits.limits.maxComputeInvocationsPerWorkgroup << std::endl;
    std::cout << " - maxComputeWorkgroupSizeX: " << limits.limits.maxComputeWorkgroupSizeX
              << std::endl;
    std::cout << " - maxComputeWorkgroupSizeY: " << limits.limits.maxComputeWorkgroupSizeY
              << std::endl;
    std::cout << " - maxComputeWorkgroupSizeZ: " << limits.limits.maxComputeWorkgroupSizeZ
              << std::endl;
    std::cout << " - maxComputeWorkgroupsPerDimension: "
              << limits.limits.maxComputeWorkgroupsPerDimension << std::endl;
  }

  wgpu::AdapterProperties properties = {};
  properties.nextInChain = nullptr;
  adapter.GetProperties(&properties);
  std::cout << "Adapter properties:" << std::endl;
  std::cout << " - vendorID: " << properties.vendorID << std::endl;
  std::cout << " - deviceID: " << properties.deviceID << std::endl;
  std::cout << " - name: " << properties.name << std::endl;
  if (properties.driverDescription) {
    std::cout << " - driverDescription: " << properties.driverDescription << std::endl;
  }

  std::cout << " - adapterType: " << as_integer(properties.adapterType) << std::endl;
  std::cout << " - backendType: " << as_integer(properties.backendType) << std::endl;
}

int main(int argc, char** argv) {
  DawnProcTable backendProcs = dawn::native::GetProcs();
  dawnProcSetProcs(&backendProcs);

  wgpu::InstanceDescriptor desc = {};
  desc.nextInChain = nullptr;

  wgpu::Instance instance = wgpu::CreateInstance(&desc);
  if (!instance) {
    std::cerr << "Could not initialize WebGPU!" << std::endl;
    return 1;
  }

  wgpu::RequestAdapterOptions adapterOpts = {};
  adapterOpts.nextInChain = nullptr;
  wgpu::Adapter adapter = requestAdapter(instance, &adapterOpts);
  if (!adapter) {
    std::cerr << "Could not request adapter!" << std::endl;
    return 1;
  }

  inspectAdapter(adapter);

  wgpu::DeviceDescriptor opts = {};
  opts.nextInChain = nullptr;
  wgpu::Device device = requestDevice(adapter, &opts);

  auto numFeatures = device.EnumerateFeatures(nullptr);
  std::cout << "Number of device features: " << numFeatures << std::endl;

  return 0;
}
