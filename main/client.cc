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
    dlog("got webgpu adapter");

    wgpu::AdapterProperties p;
    userData.adapter.GetProperties(&p);
    fprintf(stderr,
            "  %s (%s)\n"
            "    deviceID=%u, vendorID=0x%x, BackendType::%s, AdapterType::%s\n",
            p.name, p.driverDescription, p.deviceID, p.vendorID, backendTypeName(p.backendType),
            adapterTypeName(p.adapterType));

    // auto device = userData.adapter.CreateDevice();
  };

  // Call to the WebGPU request adapter procedure
  instance.RequestAdapter(options, onAdapterRequestEnded, (void*)&userData);

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
  wgpu::SwapChain swapchain;
  wgpu::RenderPipeline pipeline;
  wgpu::Instance instance;

  dawn_wire::ReservedInstance instanceReservation;
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
    auto adapter = requestAdapter(instance, &adapterOpts);

    deviceReservation = wireClient->ReserveDevice();
    device = wgpu::Device::Acquire(deviceReservation.device); // global var
    device.SetUncapturedErrorCallback(printDeviceError, nullptr);
  }

  // invoked before starting event loop
  void start(RunLoop* rl, int fd) {
    initDawnWire();
    proto.start(rl, fd);
    proto.Flush();
  }

  // runs in response to onFrame callback
  void render_frame() {
    dlog("render frame()");
    return;
  }
};

// called by main function. Sets up Connection object, proto callbacks
// and event loop, runs event loop until exit
// 3 callbacks are used: onFrame, onDawnBuffer and onFramebufferInfo
void runloop_main(int fd) {
  RunLoop* rl = EV_DEFAULT;
  FDSetNonBlock(fd);

  Connection conn;

  conn.proto.onFrame = [&]() { conn.render_frame(); };

  conn.proto.onDawnBuffer = [&](const char* data, size_t len) {
    dlog("onDawnBuffer len=%zu", len);
    assert(conn.wireClient != nullptr);
    if (conn.wireClient->HandleCommands(data, len) == nullptr) {
      dlog("wireClient->HandleCommands FAILED");
    }
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
  const char* sockfile = SERVER_SOCK;
  while (1) {
    if (first_retry) {
      dlog("connecting to UNIX socket \"%s\" ...", sockfile);
      first_retry = false;
    }
    int fd = connectUNIXSocket(sockfile);
    if (fd < 0) {
      if (errno != ECONNREFUSED && errno != ENOENT) {
        perror("connectUNIXSocket");
      }
      sleep(1);
      continue;
    }
    first_retry = true;
    dlog("connected to socket");
    double t = ev_time();
    runloop_main(fd);
    close(fd);
    t = ev_time() - t;
    if (t < 1.0) {
      sleep(1);
    }
  }
  dlog("exit");
  return 0;
}
