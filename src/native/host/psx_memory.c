/* src/native/host/psx_memory.c — see psx_memory.h. */

#define _GNU_SOURCE /* memfd_create */

#include "psx_memory.h"

#include <stdio.h>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#elif defined(__linux__)
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#else
#error "psx_memory: no KUSEG mirror for this OS"
#endif

#define WASM_PAGE 0x10000u

/* KUSEG mirror: the game stores some RAM addresses with bit 31 clear
   (collision model pointers, RelocateActorMeshHeader) and dereferences them
   through the mirror. Both linear-memory offsets 0 and PSX_RAM_BASE must name
   the same bytes. Runs before the module is instantiated, while the memory is
   still all zero. */
static int map_kuseg_mirror(uint8_t *data) {
#if defined(__APPLE__)
  /* Remap the KSEG0 pages over offset 0. */
  mach_vm_address_t mirror = (mach_vm_address_t)(uintptr_t)data;
  vm_prot_t cur, max;
  kern_return_t kr;

  kr = mach_vm_remap(mach_task_self(), &mirror, PSX_RAM_SIZE, 0,
                     VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE, mach_task_self(),
                     (mach_vm_address_t)(uintptr_t)(data + PSX_RAM_BASE),
                     FALSE, &cur, &max, VM_INHERIT_SHARE);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "psx_memory: KUSEG mirror remap failed: %s\n",
            mach_error_string(kr));
    return -1;
  }
  return 0;
#else
  /* Private anonymous pages cannot be aliased on Linux: back both views with
     one shared memory file mapped over each. */
  static const uint32_t views[] = {0, PSX_RAM_BASE};
  int fd = memfd_create("psx_ram", MFD_CLOEXEC);
  unsigned i;

  if (fd < 0 || ftruncate(fd, PSX_RAM_SIZE) != 0) {
    fprintf(stderr, "psx_memory: KUSEG mirror memfd failed: %s\n",
            strerror(errno));
    return -1;
  }
  for (i = 0; i < sizeof(views) / sizeof(views[0]); i++) {
    if (mmap(data + views[i], PSX_RAM_SIZE, PROT_READ | PROT_WRITE,
             MAP_SHARED | MAP_FIXED, fd, 0) == MAP_FAILED) {
      fprintf(stderr, "psx_memory: KUSEG mirror map failed: %s\n",
              strerror(errno));
      close(fd);
      return -1;
    }
  }
  close(fd); /* the mappings keep the file alive */
  return 0;
#endif
}

int psx_memory_init(wasm_rt_memory_t *mem) {
  uint64_t pages = (PSX_RAM_BASE + PSX_RAM_RESERVE) / WASM_PAGE;

  wasm_rt_allocate_memory(mem, pages, pages, false, WASM_PAGE);
  return map_kuseg_mirror(mem->data);
}

void psx_memory_free(wasm_rt_memory_t *mem) { wasm_rt_free_memory(mem); }
