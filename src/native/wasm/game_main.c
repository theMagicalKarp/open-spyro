/* src/native/wasm/game_main.c — the native entry point.
 *
 * Does what the PS1 BIOS does before the game runs: opens the disc, reads
 * SYSTEM's executable (SCUS_942.28) and copies its image to the address in
 * its PS-X EXE header. Under the identity map that image puts every
 * initialized global (main .rodata / .data) at its PS1 address, where the
 * game C reaches it through the GOT. The image's code bytes are inert: the
 * game's code is this module. The 26 code-pointer words in main .data all
 * belong to the SDK the module replaces (libgpu's dispatch table, crt0), so
 * nothing in the image needs patching. Overlays load the same way at run time:
 * the game reads them from WAD.WAD into 0x8007aa38 itself.
 *
 * Then calls the game's main(), which never returns. */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <psyz.h>
#include <libcd.h>

#define EXPORT(name) __attribute__((export_name(#name)))

int main(void);
int Psyz_CdSetDiskPath(const char *diskPath);
void Psyz_SpuInit(void);

#define SECTOR 2048
#define EXE_NAME "\\SCUS_942.28;1"

/* PS-X EXE header fields (offsets into the first sector). */
#define EXE_T_ADDR 0x18
#define EXE_T_SIZE 0x1C

static char arg_buffer[1024];

EXPORT(native_arg_buffer) char *native_arg_buffer(void) { return arg_buffer; }

static uint32_t rd32(const unsigned char *p) {
  return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

static int read_sectors(CdlLOC *pos, int sectors, void *dst) {
  if (!CdControlB(CdlSetloc, (u_char *)pos, NULL)) {
    return 0;
  }
  return CdRead(sectors, (u_long *)dst, 0);
}

static int load_exe(void) {
  static unsigned char header[SECTOR];
  CdlFILE file;
  CdlLOC pos;
  uint32_t t_addr, t_size;

  if (CdSearchFile(&file, EXE_NAME) == NULL) {
    printf("native: %s not found on the disc\n", EXE_NAME);
    return 0;
  }
  if (!read_sectors(&file.pos, 1, header) ||
      memcmp(header, "PS-X EXE", 8) != 0) {
    printf("native: %s is not a PS-X EXE\n", EXE_NAME);
    return 0;
  }
  t_addr = rd32(header + EXE_T_ADDR);
  t_size = rd32(header + EXE_T_SIZE);
  CdIntToPos(CdPosToInt(&file.pos) + 1, &pos);
  if (!read_sectors(&pos, (int)((t_size + SECTOR - 1) / SECTOR),
                    (void *)(uintptr_t)t_addr)) {
    printf("native: reading %s failed\n", EXE_NAME);
    return 0;
  }
  printf("native: %s image %08x+%06x loaded\n", EXE_NAME, (unsigned)t_addr,
         (unsigned)t_size);
  return 1;
}

/* arg_buffer holds the .cue path (the host copies it in). */
EXPORT(native_main) int native_main(void) {
  if (Psyz_CdSetDiskPath(arg_buffer) < 0) {
    printf("native: cannot open disc image %s\n", arg_buffer);
    return 1;
  }
  /* The SPU emulation zeroes its RAM on its first init. PsyZ starts it from
     libcd only when the game first selects CD-DA / XA audio, by which time
     the boot has uploaded the title's sample bank (menu sounds); start it
     now, as the PS1's SPU is up before the game runs. */
  Psyz_SpuInit();
  CdInit();
  if (!load_exe()) {
    return 1;
  }
  main();
  return 0;
}
