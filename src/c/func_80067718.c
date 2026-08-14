#include "globals.h"

/* Per-port "card already acknowledged" bitmask, not an op-outstanding mask:
   func_8006635C sets a port's bit when _card_info reports a new card
   (status 4), infers status 4 when status 0 arrives with the bit clear, and
   clears the bit on any error. Setting it at the top of a file op marks that
   port's card as already seen so a later card-info query reports no swap. */
extern volatile int D_80075B4C;
extern volatile int D_80075B50[]; /* [0] = op in progress, [2] = op-complete */
extern volatile int D_80075B54;   /* op result code */
extern volatile int D_80075B58;   /* op-complete flag */
extern volatile int D_80075B5C;   /* port the current op belongs to */
extern volatile int D_80075B98[]; /* result latched by the unwind */
extern int D_80075BA0;            /* saved callback-suspend state */
extern char D_80011F54[];         /* "already busy" trace */
extern char D_800120A4[];         /* "card busy" trace */

extern void func_80066634(void);
extern int func_80067614(int enable);
extern int open(char *path, unsigned int flags);
extern int close(int fd);
extern int erase(char *path);
extern int format(char *path);
extern void _new_card(void);
extern int _card_write(int port, int block, unsigned char *buf);

/* libmcrd file create (0x80067718, 560 bytes).
   Builds "bu00:<name>" for the port, fails with 6 if the file already exists,
   then creates it with the block count in the high half of the open flags.
   A create that fails to start is retried up to 16 times on result 2 (retry)
   and restarted from scratch on result 3 (card swapped). */
int func_80067718(int port, char *name, int blocks) {
  char path[0x20];
  volatile int *p = D_80075B50;
  volatile int *q;
  int result;
  /* DO NOT REMOVE — load-bearing despite being unused. Taking the address sets
     TREE_ADDRESSABLE at parse time, which commits `result` to a MEM in
     expand_decl and parks it at 0x30(sp); gcc 2.7.2 has no pass that promotes
     it back. Deleting this compiles fine and silently breaks the byte match. */
  int *rp = &result;
  int tries;
  int fd;
  int latched;

  tries = 0;
  if (p[0] != 0) {
    WritePrintf(D_800120A4);
    return -1;
  }

  FormatMemCardPath(port, path);
  strcat(path, name);
  /* Shifts by D_80075B5C, which still holds the PREVIOUS op's port — `port`
     does not reach D_80075B5C until the assignment inside the loop below. The
     original does this; it is reproduced as-is, not corrected. */
  D_80075B4C |= 1 << D_80075B5C;

  fd = open(path, 1);
  if (fd >= 0) {
    close(fd);
    return 6;
  }
  goto start;

  /* Reached only by the backward branch out of the loop, so it sits ahead of
     the loop preheader; the `goto start` above is threaded into the `bltz`. */
busy:
  return 7;

start:
  q = D_80075B50;
  for (;;) {
    fd = open(path, (blocks << 16) | 0x200);
    if (fd >= 0) {
      goto ok;
    }

    D_80075BA0 = func_80067614(0);
    if (q[0] != 0) {
      WritePrintf(D_80011F54);
    } else {
      q[0] = 2;
      D_80075B54 = 0;
      D_80075B58 = 0;
      D_80075B5C = port;
      RegisterMemCardEvent(func_80066634);
    }
    if (q[0] != 0 || q[2] != 0) {
      /* The comma operator is deliberate and load-bearing: only q[2] feeds
         the comparison, but q is volatile, so evaluating q[0] and q[1] for
         their discarded values still emits their loads. That is the point —
         the original reads all three words here before branching on the
         third. Plain `q[2] == 0` drops two loads and breaks the match. */
      if ((q[0], q[1], q[2]) == 0) {
        /* held base, not a %lo form: the spin loads through a pointer */
        volatile int *done = &D_80075B58;
        do {
        } while (*done == 0);
      }
      /* Latch first, clear second: the volatile store cannot be scheduled
         across the load, so the temp is what puts them in this order. */
      latched = D_80075B98[0];
      D_80075B58 = 0;
      result = latched;
    }
    func_80067614(D_80075BA0);

    /* `result` looks like it can be read uninitialized on the first pass: it
       is assigned only inside the `q[0] != 0 || q[2] != 0` block above. It
       cannot be. The if/else immediately before that guard leaves q[0]
       nonzero on both paths (either it already was, or it was just set to 2),
       so the guard is always taken. q is volatile, so the compiler must
       re-load q[0] at the guard and cannot prove this itself — hence the
       apparent hole. */
    if (result == 0) {
      goto busy;
    }
    if (result != 3) {
      if (result != 2) {
        break;
      }
      tries += 1;
      if (tries >= 0x10) {
        break;
      }
    }
  }

  if (result == 0) {
    result = 5;
  }
  return result;

ok:
  close(fd);
  return 0;
}

/* libmcrd file delete (0x80067948, 480 bytes).
   Same shape as the create above without the exists-check or the block count:
   erase() returning nonzero is the success path. */
int func_80067948(int port, char *name) {
  char path[0x20];
  volatile int *p = D_80075B50;
  volatile int *q;
  int result;
  /* DO NOT REMOVE — load-bearing despite being unused. Taking the address sets
     TREE_ADDRESSABLE at parse time, which commits `result` to a MEM in
     expand_decl and parks it at 0x30(sp); gcc 2.7.2 has no pass that promotes
     it back. Deleting this compiles fine and silently breaks the byte match. */
  int *rp = &result;
  int tries;
  int latched;

  tries = 0;
  if (p[0] != 0) {
    WritePrintf(D_800120A4);
    return -1;
  }
  FormatMemCardPath(port, path);
  strcat(path, name);
  q = D_80075B50;
  /* Same stale-port quirk as func_80067718: D_80075B5C still holds the previous
     op's port here, not `port`. Faithful to the original. */
  D_80075B4C |= 1 << D_80075B5C;
  for (;;) {
    result = erase(path);
    if (result != 0) {
      goto ok;
    }
    D_80075BA0 = func_80067614(0);
    if (q[0] != 0) {
      WritePrintf(D_80011F54);
    } else {
      q[0] = 2;
      D_80075B54 = 0;
      D_80075B58 = 0;
      D_80075B5C = port;
      RegisterMemCardEvent(func_80066634);
    }
    if (q[0] != 0 || q[2] != 0) {
      /* The comma operator is deliberate and load-bearing: only q[2] feeds
         the comparison, but q is volatile, so evaluating q[0] and q[1] for
         their discarded values still emits their loads. That is the point —
         the original reads all three words here before branching on the
         third. Plain `q[2] == 0` drops two loads and breaks the match. */
      if ((q[0], q[1], q[2]) == 0) {
        /* held base, not a %lo form: the spin loads through a pointer */
        volatile int *done = &D_80075B58;
        do {
        } while (*done == 0);
      }
      /* Latch first, clear second: the volatile store cannot be scheduled
         across the load, so the temp is what puts them in this order. */
      latched = D_80075B98[0];
      D_80075B58 = 0;
      result = latched;
    }
    func_80067614(D_80075BA0);
    if (result != 3) {
      if (result != 2) {
        break;
      }
      tries += 1;
      if (tries >= 0x10) {
        break;
      }
    }
  }
  if (result == 0) {
    result = 5;
  }
  return result;

ok:
  return 0;
}

/* libmcrd card format (0x80067b28, 152 bytes).
   Fire-and-wait: no retry ladder, the raw event result is mapped to an access
   code by MapMemCardAccessCode. */
int func_80067B28(int port) {
  char path[0x40];
  volatile int *p = D_80075B50;

  if (p[0] != 0) {
    WritePrintf(D_800120A4);
    return -1;
  }
  /* Stronger form of the quirk in the two functions above: this one never
     assigns D_80075B5C at all, so the bit set is always for whatever port the
     last op happened to leave behind. Faithful to the original. */
  D_80075B4C |= 1 << D_80075B5C;
  FormatMemCardPath(port, path);
  PollMemCardEvents();
  format(path);
  return MapMemCardAccessCode(WaitMemCardEventSlot2());
}

/* libmcrd card initialise (0x80067bc0, 188 bytes).
   Writes an all-0xFF 128-byte block into the first 15 directory blocks, giving
   up as soon as one write reports an error. */
int func_80067BC0(int port) {
  unsigned char buf[0x80];
  unsigned char *p;
  int i;
  int fill;

  if (D_80075B50[0] != 0) {
    WritePrintf(D_800120A4);
    return -1;
  }
  goto init;

  /* Same out-of-line stub as `busy:` in func_80067718: the backward branch out
     of the write loop reaches it, and `goto init` is threaded into the beqz. */
ret0:
  return 0;

init:
  /* One index serves both loops so they share a single allocno (s0). */
  fill = 0xFF;
  i = 0x7F;
  p = &buf[0x7F];
  do {
    *p = fill;
    i -= 1;
    p -= 1;
  } while (i >= 0);
  i = 0;
  do {
    PollMemCardEvents();
    _new_card();
    _card_write(port, i, buf);
    if (WaitMemCardEventSlot2() != 0) {
      goto ret0;
    }
    /* Increment in the condition: dbr pulls it into the branch delay slot from
       the fall-through, leaving the call's slot empty. */
  } while (++i < 0xF);
  return 1;
}
