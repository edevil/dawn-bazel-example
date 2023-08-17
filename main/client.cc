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
#include <dawn/utils/ComboRenderPipelineDescriptor.h>
#include <dawn/utils/WGPUHelpers.h>
#include <dawn/webgpu.h>
#include <dawn/wire/WireClient.h>

#include <cmath>
#include <iostream>

#include <unistd.h> // pipe

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

static void printDeviceError(WGPUErrorType errorType, const char* message, void*) {
  const char* errorTypeName = "";
  switch (errorType) {
  case WGPUErrorType_Validation:
    errorTypeName = "Validation";
    break;
  case WGPUErrorType_OutOfMemory:
    errorTypeName = "Out of memory";
    break;
  case WGPUErrorType_Unknown:
    errorTypeName = "Unknown";
    break;
  case WGPUErrorType_DeviceLost:
    errorTypeName = "Device lost";
    break;
  default:
    UNREACHABLE();
    return;
  }
  std::cerr << "device error: " << errorTypeName << " error: " << message << std::endl;
}

struct Connection {
  DawnRemoteProtocol proto;

  dawn_wire::WireClient* wireClient = nullptr;
  wgpu::Device device;
  wgpu::SwapChain swapchain;
  wgpu::RenderPipeline pipeline;

  dawn_wire::ReservedDevice deviceReservation;
  dawn_wire::ReservedSwapChain swapchainReservation;

  ~Connection() {
    // prevent double free by releasing refs to things that the wireClient owns
    if (wireClient) {
      pipeline.Release();
      device.Release();
      swapchain.Release();
      delete wireClient;
    }
  }

  void initDawnWire() {
    dawn_wire::WireClientDescriptor clientDesc = {};
    clientDesc.serializer = &proto;
    wireClient = new dawn_wire::WireClient(clientDesc); // global var

    deviceReservation = wireClient->ReserveDevice();
    device = wgpu::Device::Acquire(deviceReservation.device); // global var

    DawnProcTable procs = dawn_wire::client::GetProcs();
    procs.deviceSetUncapturedErrorCallback(device.Get(), printDeviceError, nullptr);
    dawnProcSetProcs(&procs);
  }

  void initDawnPipeline() {
    dawn::utils::ComboRenderPipelineDescriptor desc;
    desc.vertex.module = dawn::utils::CreateShaderModule(device, R"(
      let pos : array<vec2<f32>, 3> = array<vec2<f32>, 3>(
          vec2<f32>( 0.0,  0.5),
          vec2<f32>(-0.5, -0.5),
          vec2<f32>( 0.5, -0.5)
      );
      [[stage(vertex)]] fn main(
          [[builtin(vertex_index)]] VertexIndex : u32
      ) -> [[builtin(position)]] vec4<f32> {
          return vec4<f32>(pos[VertexIndex], 0.0, 1.0);
      }
    )");
    desc.cFragment.module = dawn::utils::CreateShaderModule(device, R"(
      [[stage(fragment)]] fn main() -> [[location(0)]] vec4<f32> {
          return vec4<f32>(1.0, 0.0, 0.7, 1.0);
      }
    )");
    desc.cTargets[0].format = wgpu::TextureFormat::BGRA8Unorm;

    pipeline = device.CreateRenderPipeline(&desc); // global var
  }

  void start(RunLoop* rl, int fd) {
    initDawnWire();
    initDawnPipeline();
    proto.start(rl, fd);
  }

  uint32_t fc = 0;
  bool animate = true;

  void render_frame() {
    fc++;

    float RED = 0.4;
    float GREEN = 0.4;
    float BLUE = 0.4;
    if (animate) {
      RED = std::abs(sinf(float(fc * 10) / 100));
      GREEN = std::abs(sinf(float(fc * 10) / 90));
      BLUE = std::abs(cosf(float(fc * 10) / 80));
    }

    dlog("render frame=%zu animate=%zu, R=%zf G=%zf B=%zf", fc, animate, RED, GREEN, BLUE);

    wgpu::RenderPassColorAttachment colorAttachment;
    colorAttachment.view = swapchain.GetCurrentTextureView();
    colorAttachment.clearValue = {RED, GREEN, BLUE, 0.0f};
    colorAttachment.loadOp = wgpu::LoadOp::Clear;
    colorAttachment.storeOp = wgpu::StoreOp::Store;

    wgpu::RenderPassDescriptor renderPassDesc;
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &colorAttachment;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.SetPipeline(pipeline);
    pass.Draw(3);
    pass.End();

    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);

    swapchain.Present();

    proto.Flush();
  }
};

void runloop_main(int fd) {
  RunLoop* rl = EV_DEFAULT;
  FDSetNonBlock(fd);

  Connection conn;

  conn.proto.onFrame = [&]() { conn.render_frame(); };

  conn.proto.onDawnBuffer = [&](const char* data, size_t len) {
    dlog("onDawnBuffer len=%zu", len);
    assert(conn.wireClient != nullptr);
    if (conn.wireClient->HandleCommands(data, len) == nullptr)
      dlog("wireClient->HandleCommands FAILED");
  };

  conn.proto.onFramebufferInfo = [&](const DawnRemoteProtocol::FramebufferInfo& fbinfo) {
    double dpscale = (double)fbinfo.dpscale / 1000.0;
    dlog("onFramebufferInfo %ux%u@%.2f", fbinfo.width, fbinfo.height, dpscale);

#define ENABLE_FBINFO_WORKAROUND_RESTART
#ifdef ENABLE_FBINFO_WORKAROUND_RESTART
    // XXX FIXME
    // This is a terrible and temporary fix util we can figure out how to
    // make Dawn sync and update its swapchain resevation and/or wire client
    // & server, etc. Whenever the server framebuffer changes, drop this
    // connection and restart the client with a new connection.
    if (conn.swapchain) {
      conn.proto.stop();
      return;
    }
#endif

    // [WORK IN PROGRESS] replace/update swapchain
    dlog("reserving new swapchain");
    if (conn.swapchain) {
      conn.wireClient->ReclaimSwapChainReservation(conn.swapchainReservation);
    }
    conn.swapchainReservation = conn.wireClient->ReserveSwapChain(conn.device.Get());
    conn.swapchain = wgpu::SwapChain::Acquire(conn.swapchainReservation.swapchain);
    dlog("sending swapchain reservation to server");
    conn.proto.sendReservation(conn.swapchainReservation);
  };

  conn.start(rl, fd);
  ev_run(rl, 0);
  dlog("exit runloop");
}

int main(int argc, const char* argv[]) {
  bool first_retry = true;
  const char* sockfile = "/tmp/server.sock";
  while (1) {
    if (first_retry) {
      dlog("connecting to UNIX socket \"%s\" ...", sockfile);
      first_retry = false;
    }
    int fd = connectUNIXSocket(sockfile);
    if (fd < 0) {
      if (errno != ECONNREFUSED && errno != ENOENT)
        perror("connectUNIXSocket");
      sleep(1);
      continue;
    }
    first_retry = true;
    dlog("connected to socket");
    double t = ev_time();
    runloop_main(fd);
    close(fd);
    t = ev_time() - t;
    if (t < 1.0)
      sleep(1);
  }
  dlog("exit");
  return 0;
}