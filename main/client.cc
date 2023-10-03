// Copyright 2017 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#define DLOG_PREFIX "\e[1;36m[client]\e[0m "

#include "common.hh"
#include "protocol.hh"

#include <dawn/common/Assert.h>
#include <dawn/dawn_proc.h>
#include <dawn/webgpu.h>
#include <dawn/wire/WireClient.h>

#include <iostream>

#include <unistd.h> // pipe

std::string cWGSL = R"(@group(0) @binding(0) var<storage,read> inputBuffer: array<f32,64>;
@group(0) @binding(1) var<storage,read_write> outputBuffer: array<f32,64>;

// The function to evaluate for each element of the processed buffer
fn f(x: f32) -> f32 {
    return 2.0 * x + 1.0;
}

@compute @workgroup_size(32)
fn computeStuff(@builtin(global_invocation_id) id: vec3<u32>) {
    // Apply the function f to the buffer element at index id.x:
    outputBuffer[id.x] = f(inputBuffer[id.x]);
}
)";

inline constexpr auto m_bufferSize = 64 * sizeof(float);

int connectUNIXSocket(const char* filename) {
  /*struct*/ sockaddr_un addr;
  int fd = createUNIXSocket(filename, &addr);
  if (fd > -1) {
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
      int e = errno;
      close(fd);
      errno = e;
      fd = -1;
    }
  }
  return fd;
}

wgpu::Adapter requestAdapter(wgpu::Instance instance, wgpu::RequestAdapterOptions const* options,
                             DawnRemoteProtocol& proto) {
  // A simple structure holding the local information shared with the
  // onAdapterRequestEnded callback.
  struct UserData {
    wgpu::Adapter adapter = nullptr;
    bool requestEnded = false;
  };

  // TODO: this should be allocated in the heap, since in the callback it is out of scope
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
    DawnRemoteProtocol& proto = *reinterpret_cast<DawnRemoteProtocol*>(pUserData);
    wgpu::Adapter wAdapter;
    if ((wgpu::RequestAdapterStatus)status == wgpu::RequestAdapterStatus::Success) {
      wAdapter = (wgpu::Adapter)adapter;
    } else {
      std::cout << "Could not get WebGPU adapter: " << message << std::endl;
    }
    dlog("got webgpu adapter");
    size_t count = wAdapter.EnumerateFeatures(nullptr);
    dlog("adapter number of features: %lu", count);
    std::vector<wgpu::FeatureName> features(count);
    if (count > 0) {
      wAdapter.EnumerateFeatures(features.data());
    }

    for (auto f : features) {
      auto fname = getFeatureName(f);
      if (fname) {
        dlog("Got a feature: %s", fname->c_str());
      }
    }

    wgpu::AdapterProperties p;
    wAdapter.GetProperties(&p);
    fprintf(stderr,
            "  %s (%s)\n"
            "    deviceID=%u, vendorID=0x%x, BackendType::%s, AdapterType::%s\n",
            p.name, p.driverDescription, p.deviceID, p.vendorID, backendTypeName(p.backendType),
            adapterTypeName(p.adapterType));

    wgpu::DeviceDescriptor desc{};
    wAdapter.RequestDevice(
        &desc,
        [](WGPURequestDeviceStatus status, WGPUDevice cDevice, const char* message,
           void* pUserData) {
          assert(status == WGPURequestDeviceStatus_Success);
          DawnRemoteProtocol& proto = *reinterpret_cast<DawnRemoteProtocol*>(pUserData);

          dlog("got webgpu device");
          auto device = wgpu::Device::Acquire(cDevice);
          device.SetUncapturedErrorCallback(printDeviceError, nullptr);
          device.SetLoggingCallback(printDeviceLog, nullptr);
          device.SetDeviceLostCallback(printDeviceLostCallback, nullptr);

          size_t count = device.EnumerateFeatures(nullptr);
          dlog("device number of features: %lu", count);

          // setup compute

          // Create bind group layout
          std::vector<wgpu::BindGroupLayoutEntry> bindings(2);

          // Input buffer
          bindings[0].binding = 0;
          bindings[0].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
          bindings[0].visibility = wgpu::ShaderStage::Compute;

          // Output buffer
          bindings[1].binding = 1;
          bindings[1].buffer.type = wgpu::BufferBindingType::Storage;
          bindings[1].visibility = wgpu::ShaderStage::Compute;

          wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc;
          bindGroupLayoutDesc.entryCount = (uint32_t)bindings.size();
          bindGroupLayoutDesc.entries = bindings.data();
          auto m_bindGroupLayout = device.CreateBindGroupLayout(&bindGroupLayoutDesc);

          // Load compute shader
          wgpu::ShaderModuleWGSLDescriptor shaderCodeDesc{};
          shaderCodeDesc.nextInChain = nullptr;
          shaderCodeDesc.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
          wgpu::ShaderModuleDescriptor shaderDesc{};
          shaderDesc.nextInChain = &shaderCodeDesc;
          shaderCodeDesc.code = cWGSL.c_str();

          wgpu::ShaderModule computeShaderModule = device.CreateShaderModule(&shaderDesc);

          // Create compute pipeline layout
          wgpu::PipelineLayoutDescriptor pipelineLayoutDesc;
          pipelineLayoutDesc.bindGroupLayoutCount = 1;
          pipelineLayoutDesc.bindGroupLayouts = &m_bindGroupLayout;
          auto m_pipelineLayout = device.CreatePipelineLayout(&pipelineLayoutDesc);

          // Create compute pipeline
          wgpu::ComputePipelineDescriptor computePipelineDesc;
          computePipelineDesc.compute.constantCount = 0;
          computePipelineDesc.compute.constants = nullptr;
          computePipelineDesc.compute.entryPoint = "computeStuff";
          computePipelineDesc.compute.module = computeShaderModule;
          computePipelineDesc.layout = m_pipelineLayout;
          auto m_pipeline = device.CreateComputePipeline(&computePipelineDesc);

          // Create input/output buffers
          wgpu::BufferDescriptor bufferDesc;
          bufferDesc.mappedAtCreation = false;
          bufferDesc.size = m_bufferSize;

          bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
          auto m_inputBuffer = device.CreateBuffer(&bufferDesc);

          bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
          auto m_outputBuffer = device.CreateBuffer(&bufferDesc);

          // Create an intermediary buffer to which we copy the output and that can be
          // used for reading into the CPU memory.
          bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
          auto m_mapBuffer = device.CreateBuffer(&bufferDesc);

          // Create compute bind group
          std::vector<wgpu::BindGroupEntry> entries(2);

          // Input buffer
          entries[0].binding = 0;
          entries[0].buffer = m_inputBuffer;
          entries[0].offset = 0;
          entries[0].size = m_bufferSize;

          // Output buffer
          entries[1].binding = 1;
          entries[1].buffer = m_outputBuffer;
          entries[1].offset = 0;
          entries[1].size = m_bufferSize;

          wgpu::BindGroupDescriptor bindGroupDesc;
          bindGroupDesc.layout = m_bindGroupLayout;
          bindGroupDesc.entryCount = (uint32_t)entries.size();
          bindGroupDesc.entries = entries.data();
          auto m_bindGroup = device.CreateBindGroup(&bindGroupDesc);

          // OnCompute
          auto queue = device.GetQueue();
          std::vector<float> input(m_bufferSize / sizeof(float));
          for (int i = 0; i < input.size(); ++i) {
            input[i] = 0.1f * i;
          }
          queue.WriteBuffer(m_inputBuffer, 0, input.data(), input.size() * sizeof(float));

          // Initialize a command encoder
          wgpu::CommandEncoderDescriptor encoderDesc = {};
          wgpu::CommandEncoder encoder = device.CreateCommandEncoder(&encoderDesc);

          // Create compute pass
          wgpu::ComputePassDescriptor computePassDesc;
          computePassDesc.timestampWriteCount = 0;
          computePassDesc.timestampWrites = nullptr;
          wgpu::ComputePassEncoder computePass = encoder.BeginComputePass(&computePassDesc);

          // Use compute pass
          computePass.SetPipeline(m_pipeline);
          computePass.SetBindGroup(0, m_bindGroup, 0, nullptr);

          uint32_t invocationCount = m_bufferSize / sizeof(float);
          uint32_t workgroupSize = 32;
          // This ceils invocationCount / workgroupSize
          uint32_t workgroupCount = (invocationCount + workgroupSize - 1) / workgroupSize;
          computePass.DispatchWorkgroups(workgroupCount, 1, 1);

          // Finalize compute pass
          computePass.End();

          // Before encoder.finish
          encoder.CopyBufferToBuffer(m_outputBuffer, 0, m_mapBuffer, 0, m_bufferSize);

          // Encode and submit the GPU commands
          wgpu::CommandBufferDescriptor cmdDesc{};
          wgpu::CommandBuffer commands = encoder.Finish(&cmdDesc);
          queue.Submit(1, &commands);
          dlog("submitted queue");

          // Print output
          struct MapAsyncUserData {
            wgpu::Buffer buf;
            DawnRemoteProtocol& proto;
            std::vector<float> input;
            wgpu::Device device;
          };

          auto ctx = new MapAsyncUserData{std::move(m_mapBuffer), proto, std::move(input),
                                          std::move(device)};

          ctx->buf.MapAsync(
              wgpu::MapMode::Read, 0, m_bufferSize,
              [](WGPUBufferMapAsyncStatus status, void* userdata) {
                dlog("MapAsync callback");
                // Leak here, but we need device to live long enough for the error callbacks to be
                // called
                // auto c =
                //    std::unique_ptr<MapAsyncUserData>(static_cast<MapAsyncUserData*>(userdata));
                auto c = static_cast<MapAsyncUserData*>(userdata);
                if (status == WGPUBufferMapAsyncStatus_Success) {
                  const float* output = (const float*)c->buf.GetConstMappedRange(0, m_bufferSize);
                  for (int i = 0; i < c->input.size(); ++i) {
                    std::cout << "input " << c->input[i] << " became " << output[i] << std::endl;
                  }
                  c->buf.Unmap();
                } else {
                  dlog("MapAsync callback not successful: %i", status);
                }
              },
              ctx);
          dlog("will flush");
          proto.Flush();
          dlog("flushed once");
          ctx->device.Tick();
          dlog("ticked once");
          proto.Flush();
          dlog("flushed twice");
          ctx->device.Tick();
          dlog("ticked twice");
          proto.Flush();

          dlog("flushed");
          ctx->device.Tick();
          dlog("ticked");
          proto.Flush();
          dlog("flushed");
          ctx->device.Tick();
          dlog("ticked");
          proto.Flush();
          dlog("flushed");
          ctx->device.Tick();
          dlog("ticked");
          proto.Flush();
          dlog("flushed");
          ctx->device.Tick();
          dlog("ticked");
          proto.Flush();
          dlog("flushed");
          ctx->device.Tick();
          dlog("ticked");
          proto.Flush();
          dlog("flushed");
          ctx->device.Tick();
          dlog("ticked");
          proto.Flush();
          dlog("flushed");
          ctx->device.Tick();
          dlog("ticked");
          proto.Flush();
          dlog("flushed");
          ctx->device.Tick();
          dlog("ticked");
          proto.Flush();
          dlog("flushed");
          ctx->device.Tick();
          dlog("ticked");
          proto.Flush();
          dlog("flushed");
          ctx->device.Tick();
          dlog("ticked");
          proto.Flush();
          dlog("flushed");
          ctx->device.Tick();
          dlog("ticked");
          proto.Flush();
          dlog("flushed");
          ctx->device.Tick();
          dlog("ticked");
          proto.Flush();
          dlog("flushed");
          ctx->device.Tick();
          dlog("ticked");
          proto.Flush();
          dlog("flushed");
          ctx->device.Tick();
          dlog("ticked");
          proto.Flush();
          dlog("flushed");
          ctx->device.Tick();
          dlog("ticked");
          proto.Flush();
          dlog("flushed");
          ctx->device.Tick();
          dlog("ticked");
          proto.Flush();
          dlog("flushed");
          ctx->device.Tick();
          dlog("ticked");
          proto.Flush();
          dlog("flushed");
          ctx->device.Tick();
          dlog("ticked");
          proto.Flush();
          dlog("flushed");
          ctx->device.Tick();
          dlog("ticked");
          proto.Flush();
          dlog("flushed");
          ctx->device.Tick();
          dlog("ticked");
          proto.Flush();
        },
        (void*)&proto);
    proto.Flush();
  };

  // Call to the WebGPU request adapter procedure
  instance.RequestAdapter(options, onAdapterRequestEnded, (void*)&proto);
  proto.Flush();

  // In theory we should wait until onAdapterReady has been called, which
  // could take some time (what the 'await' keyword does in the JavaScript
  // code). In practice, we know that when the wgpuInstanceRequestAdapter()
  // function returns its callback has been called.

  // TODO: Actually, now that we're using dawn wire this is not guaranteed and
  // is failing. Also need to InjectInstance from the server side with the correct
  // (id, generation)
  // assert(userData.requestEnded);

  return userData.adapter;
}

struct Connection {
  DawnRemoteProtocol proto;

  dawn_wire::WireClient* wireClient = nullptr;
  wgpu::Device device;
  wgpu::Instance instance;

  dawn_wire::ReservedInstance instanceReservation;

  ~Connection() {
    // prevent double free by releasing refs to things that the wireClient owns
    if (wireClient) {
      device.Release();
      instance.Release();
      delete wireClient;
    }
  }

  void initDawnWire() {
    DawnProcTable procs = dawn_wire::client::GetProcs();
    // procs.deviceSetUncapturedErrorCallback(device.Get(), printDeviceError, nullptr);
    dawnProcSetProcs(&procs);

    dawn_wire::WireClientDescriptor clientDesc = {};
    // serializer is configured here, optional memory transfer service
    // is not configured at this time
    clientDesc.serializer = &proto;
    wireClient = new dawn_wire::WireClient(clientDesc); // global var

    instanceReservation = wireClient->ReserveInstance();
    instance = wgpu::Instance::Acquire(instanceReservation.instance);

    wgpu::RequestAdapterOptions adapterOpts = {};
    adapterOpts.nextInChain = nullptr;
    auto adapter = requestAdapter(instance, &adapterOpts, proto);
  }

  // invoked before starting event loop
  void start(RunLoop* rl, int fd) {
    proto.start(rl, fd);
    initDawnWire();
  }
};

// called by main function. Sets up Connection object, proto callbacks
// and event loop, runs event loop until exit
// 3 callbacks are used: onFrame, onDawnBuffer and onFramebufferInfo
void runloop_main(int fd) {
  RunLoop* rl = EV_DEFAULT;
  FDSetNonBlock(fd);

  Connection conn;

  conn.proto.onFrame = [&]() {
    dlog("render frame()");
    return;
  };

  conn.proto.onDawnBuffer = [&](const char* data, size_t len) {
    dlog("onDawnBuffer len=%zu", len);
    assert(conn.wireClient != nullptr);
    if (conn.wireClient->HandleCommands(data, len) == nullptr) {
      dlog("wireClient->HandleCommands FAILED");
    }
  };

  conn.proto.onFramebufferInfo = [&](const DawnRemoteProtocol::FramebufferInfo& fbinfo) {
    dlog("onFramebufferInfo %ux%u", fbinfo.width, fbinfo.height);
  };

  conn.start(rl, fd);
  ev_run(rl, 0);
  dlog("exit runloop");
}

int main(int argc, const char* argv[]) {
  bool first_retry = true;
  const char* sockfile = SERVER_SOCK;
  int fd;
  while (1) {
    if (first_retry) {
      dlog("connecting to UNIX socket \"%s\" ...", sockfile);
      first_retry = false;
    }
    fd = connectUNIXSocket(sockfile);
    if (fd < 0) {
      if (errno != ECONNREFUSED && errno != ENOENT) {
        perror("connectUNIXSocket");
      }
      sleep(1);
      continue;
    }

    break;
  }
  dlog("connected to socket");
  runloop_main(fd);
  close(fd);

  dlog("exit");
  return 0;
}
