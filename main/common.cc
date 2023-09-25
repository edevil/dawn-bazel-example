#include "common.hh"
#include <ev.h>

const char* tmptimestamp() {
  time_t now = time(NULL);
  struct tm* tm_info = localtime(&now);
  static char buf[16]; // HH:MM:SS.nnnnnn\0
  int buftimeoffs = strftime(buf, sizeof(buf), "%H:%M:%S", tm_info);
  // .mmm
  double t = ev_time();
  double ms = (t - std::floor(t)) * 1000000.0;
  int n = snprintf(&buf[buftimeoffs], sizeof(buf) - buftimeoffs, ".%06u", (uint32_t)ms);
  buf[n + buftimeoffs] = 0;
  return buf;
}

bool FDSetNonBlock(int fd) {
  int flags = fcntl(fd, F_GETFL);
  if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0 ||
      fcntl(fd, F_SETFD, FD_CLOEXEC)) // FD_CLOEXEC for fork
  {
    errno = EWOULDBLOCK;
    return false;
  }
  return true;
}

int createUNIXSocket(const char* filename, sockaddr_un* addr) {
  addr->sun_family = AF_UNIX;
  auto filenameLen = strlen(filename);
  if (filenameLen > sizeof(addr->sun_path) - 1) {
    errno = ENAMETOOLONG;
    return -1;
  }
  memcpy(addr->sun_path, filename, filenameLen + 1);
  return socket(AF_UNIX, SOCK_STREAM, 0);
}

const char* backendTypeName(wgpu::BackendType t) {
  switch (t) {
  case wgpu::BackendType::Null:
    return "Null";
  case wgpu::BackendType::WebGPU:
    return "WebGPU";
  case wgpu::BackendType::D3D11:
    return "D3D11";
  case wgpu::BackendType::D3D12:
    return "D3D12";
  case wgpu::BackendType::Metal:
    return "Metal";
  case wgpu::BackendType::Vulkan:
    return "Vulkan";
  case wgpu::BackendType::OpenGL:
    return "OpenGL";
  case wgpu::BackendType::OpenGLES:
    return "OpenGLES";
  }
  return "?";
}

const char* adapterTypeName(wgpu::AdapterType t) {
  switch (t) {
  case wgpu::AdapterType::DiscreteGPU:
    return "DiscreteGPU";
  case wgpu::AdapterType::IntegratedGPU:
    return "IntegratedGPU";
  case wgpu::AdapterType::CPU:
    return "CPU";
  case wgpu::AdapterType::Unknown:
    return "Unknown";
  }
  return "?";
}
