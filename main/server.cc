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

#define DLOG_PREFIX "\e[1;34m[server]\e[0m "

#include "common.hh"
#include "protocol.hh"

#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>
#include <dawn/webgpu_cpp.h>
#include <dawn/wire/WireServer.h>

#include <algorithm>
#include <cmath>
#include <iostream>

#include <fcntl.h> // F_GETFL, O_NONBLOCK etc
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h> // pipe

int createUNIXSocketServer(const char* filename) {
  /*struct*/ sockaddr_un addr;
  int fd = createUNIXSocket(filename, &addr);
  if (fd > -1) {
    unlink(filename);
    int acceptQueueSize = 5;
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1 ||
        listen(fd, acceptQueueSize) == -1) {
      int e = errno;
      close(fd);
      unlink(filename);
      errno = e;
      fd = -1;
    }
  }
  return fd;
}

const char* sockfile = SERVER_SOCK;
static std::unique_ptr<dawn_native::Instance> instance;

DawnProcTable nativeProcs;
dawn_native::Adapter backendAdapter;
wgpu::Device device;
wgpu::Surface surface;

DawnRemoteProtocol::FramebufferInfo framebufferInfo = {
    .dpscale = 1000, // 1000=100%
    .width = 640,
    .height = 480,
    .textureFormat = wgpu::TextureFormat::BGRA8Unorm,
    .textureUsage = wgpu::TextureUsage::RenderAttachment,
};

void createDawnSwapChain();

// Conn is a connection to a client
struct Conn {
  uint32_t id;
  DawnRemoteProtocol _proto;
  dawn_wire::WireServer _wireServer;

  Conn(uint32_t id_) : id(id_), _wireServer({.procs = &nativeProcs, .serializer = &_proto}) {
    _proto.onDawnBuffer = [this](const char* data, size_t len) {
      dlog("onDawnBuffer len=%zu", len);
      assert(data != nullptr);
      if (_wireServer.HandleCommands(data, len) == nullptr) {
        dlog("onDawnBuffer: _wireServer.HandleCommands FAILED");
      }
      if (!_proto.Flush()) {
        dlog("_proto.Flush() FAILED");
      }
    };

    _proto.onSwapchainReservation = [this](const dawn_wire::ReservedSwapChain& scr) {
      this->onSwapchainReservation(scr);
    };
  }

  void onSwapchainReservation(const dawn_wire::ReservedSwapChain& scr) {
    dlog("onSwapchainReservation device: %u %u, swapchain %u %u\n", scr.deviceId,
         scr.deviceGeneration, scr.id, scr.generation);

    if (_wireServer.GetDevice(scr.deviceId, scr.deviceGeneration) == nullptr) {
      if (_wireServer.InjectDevice(device.Get(), scr.deviceId, scr.deviceGeneration)) {
        dlog("onSwapchainReservation _wireServer.InjectDevice OK");
      } else {
        dlog("onSwapchainReservation _wireServer.InjectDevice FAILED");
      }
    }
  }

  void start(RunLoop* rl, int fd) {
    _proto.start(rl, fd);

    // Hardcoded generation and IDs need to match what's produced by the client
    // or be sent over through the wire.
    if (_wireServer.InjectInstance(instance->Get(), 1, 0)) {
      dlog("onSwapchainReservation _wireServer.InjectInstance OK");
    } else {
      dlog("onSwapchainReservation _wireServer.InjectInstance FAILED");
    };
    sendFrameSignal();
  }

  bool sendFrameSignal() {
    if (_proto.stopped()) {
      close();
      return false;
    }
    // send FRAME message to client
    if (!_proto.sendFrameSignal()) {
      dlog("_proto.sendFrameSignal FAILED");
      close();
      return false;
    }
    return true;
  }

  void close();
};

Conn* conn0 = nullptr;

void Conn::close() {
  _proto.stop();
  if (_proto.fd() != -1) {
    ::close(_proto.fd());
  }
  if (this == conn0) {
    conn0 = nullptr;
    delete this;
  }
}

// backendType
// Default to D3D12, Metal, Vulkan, OpenGL in that order as D3D12 and Metal are
// the preferred on their respective platforms, and Vulkan is preferred to
// OpenGL
#if defined(DAWN_ENABLE_BACKEND_D3D12)
static wgpu::BackendType backendType = wgpu::BackendType::D3D12;
#elif defined(DAWN_ENABLE_BACKEND_METAL)
static wgpu::BackendType backendType = wgpu::BackendType::Metal;
#elif defined(DAWN_ENABLE_BACKEND_VULKAN)
static wgpu::BackendType backendType = wgpu::BackendType::Vulkan;
#elif defined(DAWN_ENABLE_BACKEND_OPENGL)
static wgpu::BackendType backendType = wgpu::BackendType::OpenGL;
#else
#error
#endif

// logAvailableAdapters prints a list of all adapters and their properties
void logAvailableAdapters(dawn_native::Instance* instance) {
  fprintf(stderr, "Available adapters:\n");
  for (auto&& a : instance->GetAdapters()) {
    wgpu::AdapterProperties p;
    a.GetProperties(&p);
    fprintf(stderr,
            "  %s (%s)\n"
            "    deviceID=%u, vendorID=0x%x, BackendType::%s, AdapterType::%s\n",
            p.name, p.driverDescription, p.deviceID, p.vendorID, backendTypeName(p.backendType),
            adapterTypeName(p.adapterType));
  }
}

void createDawnDevice() {
  instance = std::make_unique<dawn_native::Instance>();
  instance->DiscoverDefaultAdapters();

  logAvailableAdapters(instance.get());

  // Get an adapter for the backend to use, and create the device.
  {
    std::vector<dawn_native::Adapter> adapters = instance->GetAdapters();
    auto adapterIt = std::find_if(adapters.begin(), adapters.end(),
                                  [](const dawn_native::Adapter adapter) -> bool {
                                    wgpu::AdapterProperties properties;
                                    adapter.GetProperties(&properties);
                                    if (properties.backendType == backendType) {
                                      dlog("using adapter %s", properties.name);
                                      return true;
                                    }
                                    return false;
                                  });
    assert(adapterIt != adapters.end());
    backendAdapter = *adapterIt; // global var
  }

  // Set up the native procs for the global proctable (so calling
  // wgpu::Object::Foo calls into that proc) but also keep it around
  // so we can give it to the wire server.
  nativeProcs = dawn_native::GetProcs(); // global var
  dawnProcSetProcs(&nativeProcs);

  device = wgpu::Device::Acquire(backendAdapter.CreateDevice()); // global var

  // hook up error reporting
  device.SetUncapturedErrorCallback(printDeviceError, nullptr);
}

// onServerIO is called when a new connection is awaiting accept
static void onServerIO(RunLoop* rl, ev_io* w, int revents) {
  dlog("onServerIO called");
  int fd = accept(w->fd, NULL, NULL);
  if (fd < 0) {
    if (errno != EAGAIN) {
      perror("accept");
    }
    return;
  }
  FDSetNonBlock(fd);

  if (conn0 != nullptr) {
    dlog("second client connected; closing older client (last in wins)");
    conn0->close();
  }

  static uint32_t connIdGen = 0;
  conn0 = new Conn(connIdGen++);
  dlog("client #%u connected on fd %d", conn0->id, fd);
  conn0->start(rl, fd);
}

int main(int argc, const char* argv[]) {
  dlog("starting UNIX socket server \"%s\"", sockfile);
  int fd = createUNIXSocketServer(sockfile);
  if (fd < 0) {
    perror("createUNIXSocketServer");
    return 1;
  }

  createDawnDevice();

  RunLoop* rl = EV_DEFAULT;

  // register I/O callback for the socket file descriptor
  FDSetNonBlock(fd);
  ev_io server_fd_watcher;
  ev_io_init(&server_fd_watcher, onServerIO, fd, EV_READ);
  ev_io_start(rl, &server_fd_watcher);

  ev_run(rl, 0);

  dlog("exit");
  if (conn0) {
    conn0->close();
  }

  ev_io_stop(rl, &server_fd_watcher);
  close(fd);
  unlink(sockfile);
  return 0;
}
