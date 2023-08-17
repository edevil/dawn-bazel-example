#pragma once

#include <fcntl.h> // F_GETFL, O_NONBLOCK etc
#include <iostream>
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

const char* tmptimestamp();
bool FDSetNonBlock(int fd);
int createUNIXSocket(const char* filename, sockaddr_un* addr);
