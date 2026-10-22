/* Credits_Update (0x8007AA50, credits overlay, 5504 B).
 *
 * Credits-sequence frame update (the Draw half is Credits_Draw).
 * While the sequence gate g_nGamestate0fMode is -1 it first BUILDS the sequence:
 * clears the 100-entry glyph pool (0x1C stride, base D_8007589C =
 * Credits_OverlayEnd), loads the four credits VRAM strips through
 * CdReadSyncSectors, waits on the CD stream, relocates the loaded script
 * (D_80075708 line table, 0xC stride) and arms the scroll header
 * D_800757BC.  Then, every frame: advances the header cursor and the fade
 * level g_nGenericCountdown, walks the visible line window
 * [D_80075968 .. D_80075878], and for each newly revealed line lays its
 * text out into free glyph records -- one record per non-space character,
 * x advanced by 0xE (0x10 for digit runs, 3/4 of a space), ':' emitting two
 * dots, and the glyph index taken from the 0x30/0x41/0x61 ranges plus a
 * small punctuation table.  A second pass re-centres the finished line on
 * (x - 0x100) / 2 and seeds each glyph's per-frame drift/colour deltas, and
 * two more passes run the per-glyph state machine (1 = fly in, 3 = fall
 * out) and trim the pool tail.  Finally the 5-way g_nGamestate0fMode state machine
 * pages the next credits screen in through StartCdReadAsync / CopyWords.
 *
 * ---- STATE 2026-09-02 (first session; superseded, see 2026-10-04-1 below) --
 * PARKED at **length-exact (1372/1372), frame-exact, running offset 0**, with
 * 785/1372 linked instructions differing (42.8%) and **252 mismatches over
 * 123 regions** on the register-anchored, immediate-masked, difflib-aligned
 * shape diff.  It started the session as an auto-skipped WALL (see below) and
 * as a fresh m2c decode at 588 masked mismatches / offset -40.
 *
 * ---- Measuring this park (do this first) ---------------------------------
 *   uv run --project tools/open-spyro --group split -- open-spyro diff \
 *       Credits_Update            # ~35 s, writes build/diff/*.bin
 * then objdump BOTH build/diff/Credits_Update.{orig,built}.bin with
 * `-b binary -m mips:3000 -EL -D`, anchor register names, mask immediates and
 * difflib-align (see cookbook D-2b; because BOTH sides go through the SAME
 * objdump there is no fp/s8 or t3/t7 naming asymmetry to correct).  Print the
 * running offset per region -- it is the number that matters, not the raw
 * mismatch count.
 *
 * ---- Why it was hidden: the triage B2 detector false-positives ------------
 * `make triage` auto-skipped this function as "B2 $at literal HW load".  It is
 * not B2: every `$at` use here is a gas MACRO expansion -- `lui $at,%hi(sym)`
 * + `sw ...,%lo(sym)($at)` for a global store, `lui $at; addu $at,$at,$i;
 * lw ...,%lo(sym)($at)` for an indexed global, and `lui $at,(0x80000000>>16)`
 * + `bne $v1,$at,L` for an immediate COMPARE.  Real B2 is `lui $at,<imm>`
 * feeding a load/store whose displacement is a BARE constant
 * (`func_80017FD4`: `lui $at,(0x1F801120>>16); lhu $v0,(0x1F801120&0xFFFF)($at)`).
 * The detector now keys off the memory operand instead of the `lui`
 * (triage.py); that moved 32 overlay functions out of the wall list.
 *
 * ---- The seven levers that took it 588 -> 252 (all reusable) --------------
 * 1. **A local STRUCT is how the original keeps a group of ints in the frame.**
 *    sp+0x20/0x24/0x28/0x30/0x34/0x38 (x, y, colour, advance, 1, 0x1600) are
 *    read and written through `sp` on EVERY access with a hole at 0x2C.
 *    Separate `int` locals get hard registers; a BLKmode struct never does
 *    (gcc 2.7.2 has no SRA).  Declaring `struct { int x,y,col,unkC,adv,one,
 *    big; } st;` reproduced the whole frame window, hole included.
 * 2. **`__attribute__((noreturn))` on the pool-overflow callee.**  The one
 *    `jal exit` inside the glyph loop made every loop-carried value
 *    (slot, colon, the char pointer, the hoisted 0x10/0x100) call-crossing, so
 *    reload put them all in s-registers where the original uses a3/t0/t1/t2.
 *    `volatile void f();` is NOT enough in this cc1 -- the attribute is.
 *    Worth ~40 mismatches, and it is what put the whole caller-saved chain
 *    back.  Probe first by deleting the call: if the registers change, the
 *    callee is noreturn in the original.
 * 3. **`volatile` on a pointer/state global is the only thing that stops cse
 *    store-forwarding.**  The original re-loads g_pLevelSampleBankEntries before EVERY field
 *    store (six `lui/lw` pairs) although it had just stored a symbol address
 *    into it; ours folded the whole block to absolute `$at` addresses.  No
 *    A200 barrier reaches this (the fold is of the STORED VALUE, not of a
 *    held base).  `char *volatile g_pLevelSampleBankEntries;` fixes it in one line.  Same for
 *    g_pWorldUnpackHead and g_nLevelArchiveByteSize, which the original also re-reads after
 *    storing.  Do NOT blanket-volatile: where the original CACHES a global
 *    across a loop (D_800756E4 in the free-slot search) an explicit `int n =`
 *    local is what reproduces it, and volatile would break it.
 * 4. **gcc 2.7.2 REUSES stack temp slots across disjoint lexical blocks, so
 *    the slot map is a source-scope dial.**  The original's three rects live
 *    at sp+0x18, sp+0x20 and sp+0x28: 0x18 is a FUNCTION-scope declaration
 *    (never freed), 0x20 is the glyph struct's slot after that block exits,
 *    and 0x28 is the second rect in the same `switch` binding contour.  Moving
 *    the first rect from its `if` block to function scope moved all three onto
 *    the original's addresses.  The frame then wanted one more 8-byte slot:
 *    a dead `int padg[2];` after `st` gives `vars= 72` (`addiu sp,-136`)
 *    without moving anything else.
 * 5. **`while (i < n) { if (data) break; ... }` is NOT the same loop as
 *    `while (data) { ...; if (i >= n) break; }`.**  loop.c's
 *    `duplicate_loop_exit_test` copies the loop's FIRST exit test to the
 *    bottom, so putting the DATA test first gets it duplicated (guard + bottom
 *    copy, +6 insns and a rotated body).  Putting the COUNTER test as the loop
 *    condition and the data test as an inner `break` gives the original's
 *    "load/test at the top, `slt`+`bnez` at the bottom, increment in the delay
 *    slot" shape.  Worth 77 identical words on the free-slot search alone.
 * 6. **Sharing ONE loop counter decl across two loops is a priority dial.**
 *    The free-slot index lives in `s1` in the original -- a callee-saved
 *    register for a value that crosses no call -- because it is the SAME
 *    source variable as the re-centre pass's counter, which makes one allocno
 *    with a long live range and therefore LOW global.c priority.  With two
 *    separate `int i`s ours landed in `a1` and rotated the entire caller-saved
 *    chain by one (`n`->a2, `s`->a3, slot->t0, colon->t1, lift->t2).  One
 *    shared decl put all seven back.  Worth 33 mismatches.
 * 7. **Where the negation goes decides whether combine folds it.**
 *    `-(x - 0x100) * 3` is folded to `(0x100 - x) * 3` (one `subu`); the
 *    original's `addiu v0,v0,-256; ...*3; negu` needs `-((x - 0x100) * 3)`.
 *    Worth 34 identical words.
 * Also folded in: an A240 per-site alias for g_pDrawBufB (`D_800785DC_BLK`, an
 * incomplete-array view added to credits.undefined_syms.auto.txt) -- the setup
 * block reads it five times through one `lui/addiu` base while case 4 uses the
 * absolute `$at` form, and only two C spellings of one address give both; an
 * A232 `do { } while (0)` before the 20-byte `g_nWorldRenderMeshChunkCount = g_nWorldCollisionTriCount` struct
 * assignment (sched2 otherwise hoists a whole `move_by_pieces` block above the
 * preceding volatile store); and `if (D_8007589C[...] == 0) { tab = D_8007589C;
 * ... }` instead of a shared `tab`, which stops jump.c cross-jumping the trim
 * loop's guard INTO the loop body (§B24: the escape is different registers,
 * and a second read of the same global is how you buy them).
 *
 * ---- STATE 2026-10-04-1: 115 masked mismatches over 32 regions -----------
 * Length-exact (1372), frame-exact (136, no padding local), 1,223+ of 1,372
 * linked words byte-identical (was 587).  Measure compile-only (works when the
 * length drifts): build the .o and difflib it against
 * build/diff/<fn>.orig.bin with registers anchored and immediates masked.
 * Levers that closed R1, R2, R5 and most of R6 this session:
 *  8. **The glyph pool and the line table are STRUCT ARRAYS** (CredGlyph,
 *     CredLine).  The HImode field stores are then MEM_IN_STRUCT_P with a
 *     varying address, and sched.c true_dependence lets a SCALAR load
 *     (D_800756E4) pass them.  That is what puts `sh cur,0x18(g)` in the pool
 *     overflow branch's delay slot.  Byte fields are QImode and alias
 *     everything, as in the original.
 *  9. **Index with `POOL[i]` / `LINES[line]`, not a hand-made byte offset.**
 *     In an address (EXPAND_SUM) context, expand puts the MULT first, and
 *     loop.c's giv replacement keeps it first, which gives the original's
 *     `addu rd,giv,base`.  `&POOL[i]` in a local is NOT an address context,
 *     so it comes out base-first; where the original is giv-first but holds g
 *     in a register, write `(CredGlyph *)(i * 0x1C + (int)D_8007589C)`.  The
 *     re-centre loop and the two state loops have no `k` at all: the `move
 *     aN,zero` after each guard is loop.c's giv init.
 * 10. **Exactly ONE loop is a `for`** (the state-machine pass).  A `for`
 *     guard `i < D` with i == 0 is folded to `blez D` by combine, which
 *     eliminates the slt temp, and distribute_notes leaves `(use (reg))` for
 *     it.  That orphan pseudo gets an 8-byte stack slot.  The original's frame
 *     has exactly one such slot, which the old `int padg[2]` was faking.  The
 *     `for` guard also defeats jump threading into the next `if (D > 0)`, so
 *     cse reloads D_800756E4 after the loop as the original does.
 * 11. `cur`/`upper` (s3/s4): `upper = (unsigned)(*s - 0x30) < 0xA` expanded
 *     with `upper` as the subtraction's target, which added 2 refs at loop
 *     depth 3 and pushed upper's priority above cur's.  `c = *s - 0x30;`
 *     followed by `upper = (unsigned)c < 0xA` fixes it.  c must keep that
 *     non-byte set, otherwise combine drops the original's `andi c,0xff`.
 * 12. **A call's plain `D_8007A960[i]` argument goes in a local** (`sector`).
 *     rtx_cost of a simple MEM is COSTS_N_INSNS(1) == 2, so expand_call does
 *     not precompute it, and its load lands after the stack-arg store (a
 *     memory dependence).  Loaded first, `li a2,0x800` falls to the front and
 *     the stack store to the back, as in the original.
 * 13. The header key reads: `h + ((idx) * 3 << 3) + K` (a shift index is not
 *     a MULT rtx, so expand does not move it first).  The re-centre x uses
 *     `short gx` (unsigned-short arithmetic, reused unreloaded).
 *     g_nCdReadInFlight is `volatile`: combine will not fold `la; lw 0()` into it.
 *     Case 2's rect is written in natural [0..3] order.
 * ---- STATE 2026-10-19: 14 masked / 14 linked, one cluster left ------------
 * Length-, frame- and branch-target-exact; every difference is the four
 * movstr scratch registers of the init-block 20-byte copy (ours a2/a3/t0/t1,
 * original a3/t0/t1/t2).  Levers that closed clusters A-D and a hidden
 * jump-threading diff this session:
 * 14. **One struct symbol for the CD-stream state block** (Cd85D8 at
 *     0x800785D8, D_800785D8_S in undefined_syms_manual.txt).  The BLK copy
 *     into `S.blk` (0x80078A40) and the 85E0/85E4/8600/8604 fields share a
 *     base, so sched sees output/anti dependences between the copy and the
 *     field stores (BLKmode size 0 = "conflicts with everything on that
 *     base").  That keeps the src re-read and dc read above the copy and the
 *     8600/8604 stores below it -- no barrier, no asm.  Case 4 matched from
 *     it outright (`dc = S.dc` before `S.blk = ...`).
 * 15. **`g_nWorldTimAudioChunkOffset[3]` instead of a scalar g_nWorldBundleChunkSize** for a call argument:
 *     an ARRAY_REF address goes through memory_address, so the cheap
 *     (mem (reg)) form is NOT precomputed by expand_call and the a2 load is
 *     emitted after arg 4's D_800757AC chain, as in the original.
 * 16. `char *e4 = g_pPathTableBuffer;` captured right after the call, a `sz` local
 *     loaded before the D_800757DC store, and `int f = g_nReturningHomePhase;` before
 *     the g_pLevelSampleBankEntries re-read fix three load orders (all luid ties).
 * 17. Glyph loop: the `*s - '0'` digit test is a block temp, and the loop
 *     var gets its non-byte def from the space branch instead:
 *     `c = (st.adv * 3) / 4;` (that keeps the loop-top `andi c,0xff`).  A
 *     `do {} while (0)` after `upper = 1` stops sched1 dropping the li into
 *     the st.adv load's delay slot (which inflated the load's qty span and
 *     lost it v0 to the sll temp); a loop-note barrier, unlike an asm
 *     fence, still lets reorg put the li in the bgez delay slot.
 * 18. `i = 0;` hoisted above `if (D_800756E4 > 0)` before the first state
 *     loop.  Without it jump.c threads that blez straight past the `for`
 *     loop (target 0x8007bb6c vs 0x8007b8d0) -- invisible in the masked
 *     shape diff, caught only by the linked `open-spyro diff`.
 * ---- STATE 2026-10-19-1: 4 masked, one load-order swap left ---------------
 * 19. **The movstr scratches are allocated in SCHED1 order, and sched1 sees
 *     no hard-reg clobber on the copy.**  local-alloc gives the four scratch
 *     qtys (birth 2n-1 .. death 2n+1) the first registers free across the
 *     copy in sched1's order.  The original's a3/t0/t1/t2 means the hard a2
 *     arg (the volatile 0x8604 re-read) was already loaded ABOVE the copy in
 *     sched1; sched2, where the copy now clobbers a3, hoists it back up.
 *     Two things free sched1 to do that: the copy goes to its OWN symbol
 *     (`g_nWorldRenderMeshChunkCount = g_nWorldCollisionTriCount`, not `S.blk`, so it has no dep on the
 *     S fields), and C1C is read through a NON-volatile alias
 *     (`D_80076C1C_n`, undefined_syms_manual.txt), so the 8604 store/re-read
 *     chain is not pinned behind it.  14 -> 4 masked.  Ruled out on the way
 *     (gcc source): reload spill (a2 is regs_explicitly_used, never a spill
 *     reg) and global.c's local-reg steal (skips regs_ever_live) -- the
 *     earlier "spilled REG_EQUIV pseudo" theory for a2 is wrong.
 * ---- MATCHED 2026-10-20 ----------------------------------------------------
 * 20. **`register char *src asm("$5")` for the e4 re-read.**  sched1's
 *     adjust_priority gives a newly-ready insn the LAUNCH_PRIORITY "birthing"
 *     boost only when its SET_DEST pseudo has reg_n_sets == 1.  Both the e4
 *     re-read and the C1C load were boosted, so the class tie (anti link
 *     cost 1 beats a load's true link) put C1C first.  Setting the hard reg
 *     a1 directly (n_sets >> 1) drops the re-read to its plain priority 5;
 *     C1C wins T-67 and the original `lw a1,85E4; lw v0,C1C` order falls out.
 *     Setting a pseudo twice (`src = g_pPathTableBuffer` later) also flips the pair
 *     but makes src non-local and breaks allocation.
 */

extern void CdReadSyncSectors();
extern void CopyWords();
extern void StartCdReadAsync();
extern void RestartStalledCdRead();
extern char *UnpackWorldDataChunks();
extern void LoadImage();
extern void MoveImage();
extern void DrawSync();
extern void SetSpuTransferStartAddr();
extern void WriteSpuRam();
extern int GetSpuTransferStatus();
extern void PlaySoundEffect();
extern void RotateLightVectorXZ();
extern int TickCountdownByFrameDelta();
extern int RandomSignedInRange();
extern int RandomIntInRange();
extern void exit() __attribute__((noreturn));
extern int func_80063BD8();

typedef struct {
  int w[5];
} Blk20;

extern int g_nFrameStep;
extern int D_800756E4;
extern int g_nReturningHomePhase;
extern int g_nGamestate0fMode;
extern char *D_80075708;
extern char *D_80075768;
extern int g_nPreserveMobIdx;
extern int D_800757AC;
extern char *D_800757BC;
extern int D_800757DC;
extern char *D_800757EC;
extern int D_80075878;
extern char *D_8007589C;
extern int g_nScriptedMobCount;
extern int g_nGenericCountdown;
extern int D_80075958;
extern int D_80075968;
extern char *volatile g_pLevelSampleBankEntries;
extern int g_nSfxOverrideFlags;
extern short g_nSfxOverrideVolL;
extern short g_nSfxOverrideVolR;
extern char *g_nCdBaseLba[];
extern volatile int g_nCdReadInFlight[];
extern int g_nWorldTimAudioChunkOffset[];
extern int g_nWorldTimAudioChunkSize;
extern int g_nWorldBundleChunkOffset;
extern int g_nWorldBundleChunkSize;
extern int g_nLevelArchiveChunkOffset;
extern volatile int g_nLevelArchiveChunkSize;
extern int g_anCameraPos;
extern int g_nCameraPosY;
extern int g_nCameraPosZ;
extern short g_nCameraEulerRoll;
extern short g_nCameraEulerPitch;
extern short g_nCameraEulerYaw;
extern Blk20 g_nWorldCollisionTriCount;
extern Blk20 g_nWorldRenderMeshChunkCount;
extern int D_80076C1C_n;

extern char D_800778F0[];
extern char *g_pDrawBufA;
extern char *g_pDrawBufB;
extern char *D_800785DC_BLK[];
extern char *g_pPathTableBuffer;
extern int g_nLevelArchiveBaseLba;
extern volatile int g_nLevelArchiveByteSize;
extern int D_8007A960[];
extern char Credits_OverlayEnd[];

typedef struct {
  char *d8;
  char *dc;
  char *volatile e0;
  char *volatile e4;
  char padE8[0x18];
  int l600;
  volatile int h604;
  char pad608[0x8A40 - 0x8608];
  Blk20 blk;
} Cd85D8;
extern Cd85D8 D_800785D8_S;
#define S D_800785D8_S

/* One 0xC-byte line record of the credits script (table at D_80075708). */
typedef struct {
  unsigned char *text;
  short y;
  short unk6;
  short unk8;
  short state; /* 1 = laid out, 2 = scrolled past */
} CredLine;

/* One 0x1C-byte glyph record of the credits text pool. */
typedef struct {
  int unk0;
  unsigned char state; /* 0 free, 1 fly in, 2 settled, 3 fall out, 4 done */
  unsigned char r, g, b;
  unsigned char dr, dg, db;
  unsigned char timer;
  short dx, dy, dz;
  short x, y, z;
  short line;
  short glyph;
} CredGlyph;

#define POOL ((CredGlyph *)D_8007589C)
#define LINES ((CredLine *)D_80075708)

void Credits_Update(void) {
  short rect0[4];

  if (g_nGamestate0fMode == -1) {
    char *pool = Credits_OverlayEnd;
    int sector;
    if (D_80075768 == 0) {
      int i;
      D_8007589C = pool;
      D_800756E4 = 0;
      for (i = 0; i < 0x64; i++) {
        D_8007589C[i * 0x1C + 4] = 0;
      }
      D_80075708 = pool + 0xAF0;
      D_80075768 = pool + 0x2800;
      g_pDrawBufA = D_80075768;
      g_pDrawBufB = D_80075768;
    }
    sector = D_8007A960[D_800757AC * 2];
    CdReadSyncSectors(g_nCdBaseLba[0], D_800785DC_BLK[0], 0x800, sector, 0x258);
    CopyWords(g_nWorldTimAudioChunkOffset, D_800785DC_BLK[0], 0x1D0);
    CdReadSyncSectors(g_nCdBaseLba[0], D_800785DC_BLK[0], 0x60000,
                  g_nWorldTimAudioChunkOffset[0] + D_8007A960[D_800757AC * 2], 0x258);
    rect0[0] = 0x200;
    rect0[2] = 0x200;
    rect0[1] = 0;
    rect0[3] = 0x180;
    LoadImage(rect0, D_800785DC_BLK[0]);
    CdReadSyncSectors(g_nCdBaseLba[0], D_800785DC_BLK[0], g_nWorldTimAudioChunkSize - 0x60000,
                  g_nWorldTimAudioChunkOffset[0] + D_8007A960[D_800757AC * 2] + 0x60000, 0x258);
    SetSpuTransferStartAddr(0x1010);
    WriteSpuRam(D_800785DC_BLK[0], 0x7EFF0);
    while (GetSpuTransferStatus(0) == 0) {
    }
    CdReadSyncSectors(g_nCdBaseLba[0], D_800785DC_BLK[0], g_nWorldTimAudioChunkOffset[3],
                  g_nWorldBundleChunkOffset + D_8007A960[D_800757AC * 2], 0x258);
    S.e0 = UnpackWorldDataChunks(D_800785DC_BLK[0], 1);
    {
      register char *src asm("$5");
      S.e4 = S.e0;
      src = S.e4;
      g_nWorldRenderMeshChunkCount = g_nWorldCollisionTriCount;
      S.l600 = g_nLevelArchiveChunkOffset;
      S.h604 = D_80076C1C_n;
      g_nScriptedMobCount = 0;
      CdReadSyncSectors(g_nCdBaseLba[0], src, S.h604,
                    S.l600 + D_8007A960[D_800757AC * 2], 0x258);
    }
    {
    char *e4 = g_pPathTableBuffer;
    g_pLevelSampleBankEntries = D_800778F0;
    *(int *)g_pLevelSampleBankEntries = 0x1010;
    *(int *)(g_pLevelSampleBankEntries + 4) = -1;
    *(short *)(g_pLevelSampleBankEntries + 8) = 0x50;
    *(short *)(g_pLevelSampleBankEntries + 0xA) = 0x659;
    *(short *)(g_pLevelSampleBankEntries + 0xC) = 0;
    *(short *)(g_pLevelSampleBankEntries + 0xE) = 0;
    D_800757BC = e4;
    *(int *)(g_pLevelSampleBankEntries + 0x10) = 0;
    g_nSfxOverrideFlags = 1;
    g_nSfxOverrideVolL = 0x3FFF;
    g_nSfxOverrideVolR = 0x3FFF;
    PlaySoundEffect(0, 0, 0x10, 0);
    }
    {
      char *hdr;
      char *shift;
      hdr = g_pPathTableBuffer + *(int *)(D_800757BC + 4);
      {
      int sz;
      D_800757EC = hdr;
      sz = *(int *)hdr;
      shift = D_80075708 + *(int *)(hdr + 4) - 0xC;
      D_800757DC = *(int *)(hdr + 8);
      CopyWords(D_80075708, hdr + 0xC, sz - 0xC);
      }
      {
        int n = D_800757DC;
        int i = 0;
        if (n > 0) {
          int *p = (int *)D_80075708;
          do {
            p[0] = (int)shift + p[0];
            i += 1;
            p += 3;
          } while (i < n);
        }
      }
    }
    g_nGamestate0fMode = 0;
    D_800757AC += 1;
  }
  if (D_800757BC != 0) {
    int gate = g_nGamestate0fMode;
    if (gate >= 0) {
      int *rec = (int *)D_800757BC;
      int t = rec[0] + g_nFrameStep;
      int lim = rec[2] * 2;
      rec[0] = t;
      if (t < lim) {
        if (t < 0x20 || (t = lim - t, t < 0x20)) {
          g_nGenericCountdown = 0x10 - (t >> 1);
        } else {
          g_nGenericCountdown = 0;
        }
        if (g_nGenericCountdown >= 0x10) {
          g_nGenericCountdown = 0xF;
        }
        if (g_nGenericCountdown < 0) {
          g_nGenericCountdown = 0;
        }
        {
          char *h = D_800757BC;
          g_anCameraPos = *(int *)((*(int *)h >> 1) * 0x18 + h + 0xC);
          g_nCameraPosY = *(int *)(h + ((*(int *)h >> 1) * 3 << 3) + 0x10);
          g_nCameraPosZ = *(int *)(h + ((*(int *)h >> 1) * 3 << 3) + 0x14);
          g_nCameraEulerRoll = *(int *)(h + ((*(int *)h >> 1) * 3 << 3) + 0x18);
          g_nCameraEulerPitch = *(int *)(h + ((*(int *)h >> 1) * 3 << 3) + 0x1C);
          g_nCameraEulerYaw = *(int *)(h + ((*(int *)h >> 1) * 3 << 3) + 0x20);
        }
      } else {
        g_nGamestate0fMode = gate + 1;
      }
    }
  }
  if (D_800756E4 == 0) {
    if (D_80075968 >= D_800757DC) {
      char *p;
      *(short *)(g_pLevelSampleBankEntries + 8) -= 0xA;
      {
        int f = g_nReturningHomePhase;
        int w;
        p = g_pLevelSampleBankEntries;
        w = *(short *)(p + 8);
        g_nReturningHomePhase = f + g_nFrameStep;
        if (w < 0) {
          *(short *)(p + 8) = 0;
        }
      }
      g_nGenericCountdown = g_nReturningHomePhase;
      if (g_nReturningHomePhase >= 0x10) {
        g_nGenericCountdown = 0xF;
      }
      if (g_nGenericCountdown == 0xF) {
        g_nGamestate0fMode = 0x63;
      }
    }
  }
  if (D_800756E4 > 0 || (D_800757DC > 0 && D_80075968 < D_800757DC)) {
    int last = D_80075878;
    int line;
    int upper;
    int i;
    RotateLightVectorXZ(3);
    line = D_80075968;
    upper = 1;
    if (last >= line) {
      do {
        int cur = -1;
        if (TickCountdownByFrameDelta(&LINES[line].unk6, 2) != 0) {
          LINES[line].state = 2;
          if (LINES[line].unk8 == 0 && line == D_80075968) {
            D_80075968 = line + 1;
          }
        }
        if (LINES[0].state == 0) {
          cur = 0;
        } else if (TickCountdownByFrameDelta(&LINES[line].unk8, 2) != 0 &&
                   LINES[line + 1].state == 0 &&
                   line < D_800757DC - 1) {
          cur = line + 1;
        }
        if (cur != -1) {
          int slot = 0;
          int colon = 0;
          struct {
            int x;
            int y;
            int col;
            int unkC;
            int adv;
            int one;
            int big;
          } st;
          int n;
          int lift;
          int y0;
          unsigned char *s;
          int c;
          int x0;
          LINES[cur].state = 1;
          lift = 0x10;
          if (D_80075878 < cur) {
            D_80075878 = cur;
          }
          if (D_80075878 > D_800757DC) {
            D_80075878 = D_800757DC;
          }
          st.x = 0x100;
          y0 = LINES[cur].y;
          st.col = 0x12B3;
          st.adv = 0xE;
          st.one = 1;
          st.big = 0x1600;
          st.y = y0;
          s = LINES[cur].text;
          c = *s;
          x0 = 0x100;
          while (c != 0) {
            if ((c & 0xFF) != 0x20) {
              CredGlyph *g;
            retry: {
              n = D_800756E4;
              i = slot;
              if (i < n) {
                while (i < n) {
                  if (POOL[i].state == 0) {
                    break;
                  }
                  i += 1;
                }
                if (i < D_800756E4) {
                  slot = i;
                  g = &POOL[i];
                  goto got;
                }
              }
              g = &POOL[D_800756E4];
              D_800756E4 = D_800756E4 + 1;
            got:;
            }
              c = *s;
              if ((unsigned int)(c - 0x41) < 0x1A) {
                upper = 1;
              }
              if ((unsigned int)(c - 0x61) < 0x1A) {
                upper = 0;
              }
              g->state = 1;
              g->line = cur;
              if (D_800756E4 >= 0x65) {
                exit(0);
              }
              g->x = st.x;
              g->y = st.y;
              g->z = st.col;
              if (upper == 0) {
                g->y = g->y + st.one;
                g->z = st.big;
              }
              c = *s;
              if ((unsigned int)(c - 0x30) < 0xA) {
                g->glyph = c + 0xD4;
              } else if ((unsigned int)(c - 0x41) < 0x1A) {
                g->glyph = c + 0x169;
              } else if ((unsigned int)(c - 0x61) < 0x1A) {
                g->glyph = c + 0x149;
              } else {
                int k = c & 0xFF;
                if (k == 0x21) {
                  g->glyph = 0x4B;
                } else if (k == 0x2C) {
                  g->glyph = 0x4C;
                } else if (k == 0x2E) {
                  g->glyph = 0x147;
                } else if (k == 0x2D) {
                  g->glyph = 0x115;
                } else if (k == 0x3A) {
                  g->glyph = 0x147;
                  g->z = g->z + 0x578;
                  if (colon == 0) {
                    colon = 1;
                    goto retry;
                  }
                  colon = 0;
                  g->y = g->y - 6;
                } else {
                  g->glyph = 0x4C;
                  g->y =
                      g->y - (st.adv * 2) / 3;
                }
              }
              if (upper != 0) {
                st.x = st.x + lift;
              } else {
                st.x = st.x + st.adv;
              }
              {
                int t = *s - 0x30;
                upper = (unsigned int)t < 0xA;
              }
            } else {
              upper = 1;
              do {
              } while (0);
              c = (st.adv * 3) / 4;
              st.x = st.x + c;
            }
            s += 1;
            c = *s;
          }
          {
            int mid = (st.x - x0) >> 1;
            int d = 0x1E;
            i = 0;
            if (D_800756E4 > 0) {
              do {
                CredGlyph *g = (CredGlyph *)(i * 0x1C + (int)D_8007589C);
                if (g->line == cur) {
                  short gx = g->x - mid;
                  int dx = -((gx - 0x100) * 3) / d;
                  int dy = -(g->z * 2) / d;
                  g->x = gx;
                  g->dx = dx;
                  g->dz = dy;
                  g->x = gx - dx * d;
                  g->z = g->z - dy * d;
                  POOL[i].dr = RandomSignedInRange(4, 0xF);
                  POOL[i].dg = RandomSignedInRange(4, 0xF);
                  POOL[i].db = RandomSignedInRange(4, 0xF);
                  POOL[i].r =
                      -(int)POOL[i].dr * d;
                  POOL[i].g =
                      -(int)POOL[i].dg * d;
                  POOL[i].b =
                      -(int)POOL[i].db * d;
                  POOL[i].timer = d;
                }
                i += 1;
              } while (i < D_800756E4);
            }
          }
        }
        line += 1;
      } while (last >= line);
    }
    i = 0;
    if (D_800756E4 > 0) {
      do {
        int st = LINES[POOL[i].line].state;
        if (st == 2) {
          if (POOL[i].state == st) {
            POOL[i].state = 3;
          } else if (POOL[i].state == 4) {
            POOL[i].state = 0;
          }
        }
        i += 1;
      } while (i < D_800756E4);
    }
    for (i = 0; i < D_800756E4; i++) {
      {
        CredGlyph *g = (CredGlyph *)(i * 0x1C + (int)D_8007589C);
        switch (g->state) {
        case 1:
          g->x =
              g->x + g->dx;
          g->r = g->r + g->dr;
          g->z =
              g->z + g->dz;
          POOL[i].g = POOL[i].g + POOL[i].dg;
          POOL[i].b = POOL[i].b + POOL[i].db;
          if ((unsigned char)(POOL[i].timer =
                                  POOL[i].timer - 1) == 0) {
            POOL[i].state = 2;
            POOL[i].dx = RandomSignedInRange(3, 0xB);
            POOL[i].dy = -RandomIntInRange(6, 0xA);
            POOL[i].dz = RandomIntInRange(-0x23, 0x23);
            POOL[i].dr = RandomSignedInRange(4, 9);
            POOL[i].dg = RandomSignedInRange(4, 9);
            POOL[i].db = RandomSignedInRange(4, 9);
          }
          break;
        case 3:
          g->x =
              g->x + g->dx;
          g->y =
              g->y + g->dy;
          g->z =
              g->z + g->dz;
          g->r = g->r + g->dr;
          g->dy = g->dy + 1;
          POOL[i].g = POOL[i].g + POOL[i].dg;
          POOL[i].b = POOL[i].b + POOL[i].db;
          if (POOL[i].dy >= 0x15) {
            POOL[i].dy = 0x14;
          }
          if (POOL[i].y >= 0x119) {
            POOL[i].state = 4;
          }
          break;
        }
      }
    }
    if (D_800756E4 > 0) {
      char *tab;
      if (D_8007589C[D_800756E4 * 0x1C - 0x18] == 0) {
        tab = D_8007589C;
        for (;;) {
          int m = D_800756E4 - 1;
          D_800756E4 = m;
          if (m <= 0) {
            break;
          }
          if (tab[m * 0x1C - 0x18] != 0) {
            break;
          }
        }
      }
    }
  }
  {
    int st = g_nGamestate0fMode;
    if (st != -1) {
      if (st > 0) {
        RestartStalledCdRead();
        if (g_nCdReadInFlight[0] != 0) {
          return;
        }
        if (func_80063BD8(1, 0) != 2) {
          return;
        }
        st = g_nGamestate0fMode;
      }
      if ((unsigned int)st < 5) {
        switch (st) {
        case 0: {
          int sector = D_8007A960[D_800757AC * 2];
          StartCdReadAsync(g_nCdBaseLba[0], D_800757EC, 0x800, sector, 0x258);
          g_nGamestate0fMode = g_nGamestate0fMode + 1;
          break;
        }
        case 1:
          CopyWords(g_nWorldTimAudioChunkOffset, D_800757EC, 0x1D0);
          StartCdReadAsync(g_nCdBaseLba[0], D_800757EC, 0x20000,
                        g_nWorldTimAudioChunkOffset[0] + D_8007A960[D_800757AC * 2], 0x258);
          g_nGamestate0fMode = g_nGamestate0fMode + 1;
          break;
        case 2: {
          short rect[4];
          int span;
          rect[0] = 0x200;
          rect[1] = 0x180;
          rect[2] = 0x200;
          rect[3] = 0x80;
          DrawSync(0);
          LoadImage(rect, D_800757EC);
          DrawSync(0);
          {
            int off = g_nLevelArchiveChunkSize + 0x40000;
            span = g_nWorldBundleChunkSize + off;
          }
          D_80075958 = span;
          StartCdReadAsync(g_nCdBaseLba[0], D_800757EC, span,
                        g_nWorldTimAudioChunkOffset[0] + D_8007A960[D_800757AC * 2] + 0x20000,
                        0x258);
          g_nGamestate0fMode = g_nGamestate0fMode + 1;
          break;
        }
        case 3:
          g_nPreserveMobIdx = 0;
          break;
        case 4: {
          short rect[4];
          rect[0] = 0x200;
          rect[1] = 0x80;
          rect[2] = 0x200;
          rect[3] = 0x100;
          g_pDrawBufA = D_80075768;
          g_pDrawBufB = D_80075768;
          DrawSync(0);
          LoadImage(rect, D_800757EC);
          rect[0] = 0x200;
          rect[1] = 0x180;
          rect[2] = 0x200;
          rect[3] = 0x80;
          MoveImage(rect, 0x200, 0);
          DrawSync(0);
          CopyWords(D_80075768, D_800757EC + 0x40000, D_80075958 - 0x40000);
          D_800757EC = D_80075768 + D_80075958 - 0x40000;
          S.e0 = UnpackWorldDataChunks(g_pDrawBufB, 1);
          {
          char *dc = S.dc;
          S.blk = g_nWorldCollisionTriCount;
          g_pPathTableBuffer = dc + g_nWorldBundleChunkSize;
          }
          D_800757BC = g_pPathTableBuffer;
          g_nLevelArchiveByteSize = g_nLevelArchiveChunkSize;
          g_nLevelArchiveBaseLba = g_nLevelArchiveChunkOffset;
          if (D_800757AC % 10 < 9) {
            g_nGamestate0fMode = 0;
          } else {
            g_nGamestate0fMode = 3;
          }
          D_800757AC += 1;
          break;
        }
        }
      }
    }
  }
}
