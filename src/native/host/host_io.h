/* src/native/host/host_io.h — the host side of the module's libc services
 * (module "host", imports.c), on host pointers: the file-handle table, the
 * memory-card path copy and the log filter. Free of wasm2c and SDL so
 * native/tests can test it (host_io_test.c). */

#ifndef OPEN_SPYRO_HOST_IO_H
#define OPEN_SPYRO_HOST_IO_H

#include <stdio.h>

/* Open streams the module can hold at once. */
#define HOST_IO_MAX_FILES 32

/* fopen into the lowest free handle; -1 when it fails or none is free. */
int host_io_open(const char *path, const char *mode);
/* The stream for `handle`, or NULL when it is not open. */
FILE *host_io_file(unsigned handle);
/* fclose and free the handle; -1 when it is not open. */
int host_io_close(unsigned handle);

/* Copies `path` (with its NUL) into buf of `len` bytes: 1, or 0 when path is
   NULL or does not fit. */
int host_io_copy_path(const char *path, char *buf, unsigned len);

/* Writes "[L]msg\n" to `out` (L one of D I W E) when level is a known level
   at or above min_level; returns whether it wrote. */
int host_io_log(FILE *out, unsigned level, int min_level, const char *msg);

#endif
