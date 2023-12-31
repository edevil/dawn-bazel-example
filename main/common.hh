#pragma once

#include <dawn/common/Assert.h>
#include <dawn/webgpu_cpp.h>
#include <fcntl.h> // F_GETFL, O_NONBLOCK etc
#include <iostream>
#include <optional>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h> // strftime

#ifdef DEBUG
#define dlog(format, ...)                                                                          \
  ({                                                                                               \
    fprintf(stderr, "%s " DLOG_PREFIX format " \e[2m(%s %d)\e[0m\n", tmptimestamp(),               \
            ##__VA_ARGS__, __FUNCTION__, __LINE__);                                                \
    fflush(stderr);                                                                                \
  })
#define errlog(format, ...)                                                                        \
  (({                                                                                              \
    fprintf(stderr, "E " format " (%s:%d)\n", ##__VA_ARGS__, __FILE__, __LINE__);                  \
    fflush(stderr);                                                                                \
  }))
#else
#define dlog(...)                                                                                  \
  do {                                                                                             \
  } while (0)
#define errlog(format, ...)                                                                        \
  (({                                                                                              \
    fprintf(stderr, "E " format "\n", ##__VA_ARGS__);                                              \
    fflush(stderr);                                                                                \
  }))
#endif

#define SERVER_SOCK "/tmp/server.sock"

const char* tmptimestamp();
bool FDSetNonBlock(int fd);
int createUNIXSocket(const char* filename, sockaddr_un* addr);
const char* backendTypeName(wgpu::BackendType t);
const char* adapterTypeName(wgpu::AdapterType t);
void printDeviceError(WGPUErrorType errorType, const char* message, void*);
void printDeviceLog(WGPULoggingType logType, const char* message, void*);
void printDeviceLostCallback(WGPUDeviceLostReason reason, const char* message, void*);
std::optional<std::string> getFeatureName(wgpu::FeatureName& feature);