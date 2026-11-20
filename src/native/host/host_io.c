/* src/native/host/host_io.c — see host_io.h. */

#include "host_io.h"

#include <string.h>

static FILE *files[HOST_IO_MAX_FILES];

int host_io_open(const char *path, const char *mode) {
  for (int i = 0; i < HOST_IO_MAX_FILES; i++) {
    if (files[i] == NULL) {
      files[i] = fopen(path, mode);
      return files[i] ? i : -1;
    }
  }
  return -1;
}

FILE *host_io_file(unsigned handle) {
  return handle < HOST_IO_MAX_FILES ? files[handle] : NULL;
}

int host_io_close(unsigned handle) {
  FILE *f = host_io_file(handle);

  if (f == NULL) {
    return -1;
  }
  files[handle] = NULL;
  return fclose(f);
}

int host_io_copy_path(const char *path, char *buf, unsigned len) {
  if (path == NULL || strlen(path) >= len) {
    return 0;
  }
  strcpy(buf, path);
  return 1;
}

int host_io_log(FILE *out, unsigned level, int min_level, const char *msg) {
  static const char levels[] = "DIWE";

  if (level >= 4 || (int)level < min_level) {
    return 0;
  }
  fprintf(out, "[%c]%s\n", levels[level], msg);
  return 1;
}
