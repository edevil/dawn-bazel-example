#include "common.hh"
#include <ev.h>
#include <optional>

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

void printDeviceError(WGPUErrorType errorType, const char* message, void*) {
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

void printDeviceLog(WGPULoggingType logType, const char* message, void*) {
  const char* logTypeName = "";
  switch (logType) {

  case WGPULoggingType_Verbose:
    logTypeName = "VERBOSE";
    break;
  case WGPULoggingType_Info:
    logTypeName = "INFO";
    break;
  case WGPULoggingType_Warning:
    logTypeName = "WARNING";
    break;
  case WGPULoggingType_Error:
    logTypeName = "ERROR";
    break;
  case WGPULoggingType_Force32:
    logTypeName = "FORCE32";
    break;
  default:
    UNREACHABLE();
    return;
  }

  std::cerr << "device log: " << logTypeName << " message: " << message << std::endl;
}

void printDeviceLostCallback(WGPUDeviceLostReason reason, const char* message, void*) {
  const char* reasonName = "";

  switch (reason) {
  case WGPUDeviceLostReason_Undefined:
    reasonName = "Undefined";
    break;
  case WGPUDeviceLostReason_Destroyed:
    reasonName = "Destroyed";
    break;
  case WGPUDeviceLostReason_Force32:
    reasonName = "Force32";
    break;
  default:
    UNREACHABLE();
    return;
  }

  std::cerr << "device lost reason: " << reasonName << " message: " << message << std::endl;
}

std::optional<std::string> getFeatureName(wgpu::FeatureName& feature) {
  switch (feature) {
  case wgpu::FeatureName::DepthClipControl:
    return std::optional<std::string>{"depth-clip-control"};
  case wgpu::FeatureName::Depth32FloatStencil8:
    return std::optional<std::string>{"depth32float-stencil8"};
  case wgpu::FeatureName::TextureCompressionBC:
    return std::optional<std::string>{"texture-compression-bc"};
  case wgpu::FeatureName::TextureCompressionETC2:
    return std::optional<std::string>{"texture-compression-etc2"};
  case wgpu::FeatureName::TextureCompressionASTC:
    return std::optional<std::string>{"texture-compression-astc"};
  case wgpu::FeatureName::TimestampQuery:
    return std::optional<std::string>{"timestamp-query"};
  case wgpu::FeatureName::IndirectFirstInstance:
    return std::optional<std::string>{"indirect-first-instance"};
  case wgpu::FeatureName::ShaderF16:
    return std::optional<std::string>{"shader-f16"};
  case wgpu::FeatureName::RG11B10UfloatRenderable:
    return std::optional<std::string>{"rg11b10ufloat-renderable"};
  case wgpu::FeatureName::BGRA8UnormStorage:
    return std::optional<std::string>{"bgra8unorm-storage"};
  case wgpu::FeatureName::Float32Filterable:
    return std::optional<std::string>{"float32-filterable"};
  default:
    break;
  }

  return std::nullopt;
}
