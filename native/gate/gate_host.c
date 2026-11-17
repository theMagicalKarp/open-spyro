/* native/gate/gate_host.c — host side of the ILP32 gate.
 *
 *   gate <ram_in.bin> <mesh_chunk.bin> <archive.bin> <ram_out.bin>
 *
 * Loads 2 MiB of PS1 RAM (world bundle already in place) at 0x80000000, runs
 * the loader steps in gate_wasm.c, dropping the mesh chunk and level archive
 * in at the addresses the native loader computes (as the CD stream would),
 * and writes the RAM back out. `open-spyro native gate` drives it. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gate_w.h"
#include "psx_memory.h"

struct w2c_env {
  wasm_rt_memory_t memory;
  wasm_rt_funcref_table_t table;
  u32 memory_base;
  u32 table_base;
  u32 stack_pointer;
};

wasm_rt_memory_t *w2c_env_memory(struct w2c_env *e) { return &e->memory; }
wasm_rt_funcref_table_t *
w2c_env_0x5F_indirect_function_table(struct w2c_env *e) {
  return &e->table;
}
u32 *w2c_env_0x5F_memory_base(struct w2c_env *e) { return &e->memory_base; }
u32 *w2c_env_0x5F_table_base(struct w2c_env *e) { return &e->table_base; }
u32 *w2c_env_0x5F_stack_pointer(struct w2c_env *e) {
  return &e->stack_pointer;
}

static long read_file(const char *path, void *dst, long max) {
  FILE *f = fopen(path, "rb");
  long n;

  if (f == NULL) {
    perror(path);
    exit(2);
  }
  n = (long)fread(dst, 1, (size_t)max, f);
  fclose(f);
  return n;
}

static void place(struct w2c_env *e, const char *path, u32 addr) {
  long n = read_file(path, psx_ptr(&e->memory, addr),
                     (long)(PSX_RAM_BASE + PSX_RAM_SIZE - addr));
  printf("  %s -> %#010x (%#lx bytes)\n", path, addr, n);
}

int main(int argc, char **argv) {
  static struct w2c_env env;
  static w2c_gate gate;
  struct w2c_GOT0x2Emem *got = NULL;
  u32 head, archive, actors;
  FILE *out;

  if (argc != 5) {
    fprintf(stderr, "usage: gate <ram_in> <mesh_chunk> <archive> <ram_out>\n");
    return 2;
  }
  wasm_rt_init();
  if (psx_memory_init(&env.memory) != 0) {
    return 1;
  }
  wasm_rt_allocate_funcref_table(&env.table, NATIVE_TABLE_SIZE,
                                 NATIVE_TABLE_SIZE);
  env.memory_base = NATIVE_MODULE_BASE;
  env.table_base = NATIVE_TABLE_BASE;
  env.stack_pointer = NATIVE_STACK_TOP;

  read_file(argv[1], psx_ptr(&env.memory, PSX_RAM_BASE), PSX_RAM_SIZE);
  /* No initialized data in the gate module, so wasm-ld emits no
     __wasm_apply_data_relocs / __wasm_call_ctors to run here. */
  wasm2c_gate_instantiate(&gate, got, &env);

  if (!w2c_gate_gate_mirror(&gate, (int)(PSX_RAM_BASE + PSX_RAM_SIZE - 4))) {
    fprintf(stderr, "gate: KUSEG mirror does not alias KSEG0\n");
    return 1;
  }
  memset(psx_ptr(&env.memory, PSX_RAM_BASE + PSX_RAM_SIZE - 4), 0, 4);
  printf("  KUSEG mirror OK\n");

  head = (u32)w2c_gate_gate_run(&gate, 1);
  printf("  UnpackWorldDataChunks -> %#010x\n", head);
  place(&env, argv[2], head);
  archive = (u32)w2c_gate_gate_run(&gate, 2);
  printf("  RelocateActorMeshHeader x N -> archive at %#010x\n", archive);
  place(&env, argv[3], archive);
  actors = (u32)w2c_gate_gate_run(&gate, 3);
  printf("  BuildLoadedLevelRuntime -> actors at %#010x\n", actors);

  out = fopen(argv[4], "wb");
  if (out == NULL) {
    perror(argv[4]);
    return 2;
  }
  fwrite(psx_ptr(&env.memory, PSX_RAM_BASE), 1, PSX_RAM_SIZE, out);
  fclose(out);

  wasm2c_gate_free(&gate);
  wasm_rt_free_funcref_table(&env.table);
  psx_memory_free(&env.memory);
  wasm_rt_free();
  return 0;
}
