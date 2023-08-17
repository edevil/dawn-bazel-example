#include "pipe.hh"
#include "debug.hh"
#include <stdio.h>

void _PipeTrace(const char* name, const char* prefix, const char* data, size_t datalen) {
  char* buf = (char*)malloc(datalen * 5);
  ssize_t n = data != nullptr ? debugFmtBytes(buf, datalen * 5, data, datalen) : 0;
  if (n != -1) {
    if (n > 80) {
      fprintf(stderr, "%s  %s  %zu\n\"%s\"\n", name, prefix, datalen, buf);
    } else {
      fprintf(stderr, "%s  %s  %zu  \"%s\"\n", name, prefix, datalen, buf);
    }
  }
  free(buf);
}
