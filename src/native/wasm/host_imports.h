/* src/native/wasm/host_imports.h — functions the wasm32 module imports from
 * the native host (module "host"; implemented in src/native/host/imports.c).
 *
 * Pointers are wasm linear-memory addresses; the host translates them. File
 * handles are small host-side integers. */

#ifndef OPEN_SPYRO_HOST_IMPORTS_H
#define OPEN_SPYRO_HOST_IMPORTS_H

#define HOST_IMPORT(name)                                                      \
  __attribute__((import_module("host"), import_name(#name)))

HOST_IMPORT(write) int host_write(int fd, const void *buf, int len);
HOST_IMPORT(log) void host_log(int level, const char *msg);
HOST_IMPORT(exit) _Noreturn void host_exit(int code);
HOST_IMPORT(now_us) unsigned int host_now_us(void);

HOST_IMPORT(fopen) int host_fopen(const char *path, const char *mode);
HOST_IMPORT(fclose) int host_fclose(int h);
HOST_IMPORT(fread) int host_fread(void *dst, int len, int h);
HOST_IMPORT(fwrite) int host_fwrite(const void *src, int len, int h);
HOST_IMPORT(fseek) int host_fseek(int h, long off, int whence);
HOST_IMPORT(ftell) long host_ftell(int h);
HOST_IMPORT(file_size) int host_file_size(const char *path);
HOST_IMPORT(mkdir) int host_mkdir(const char *path);

/* The memory-card image file for `port` (0 = A, 1 = B) into buf; 0 when the
   port has no card. */
HOST_IMPORT(card_path) int host_card_path(int port, char *buf, int len);

#endif
