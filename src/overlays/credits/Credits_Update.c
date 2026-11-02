/* Credits_Update (0x8007AA50, credits overlay, 5504 B).
 *
 * Credits-sequence frame update (the Draw half is Credits_Draw).
 * While the sequence gate g_nCreditsMode is -1 it first BUILDS the sequence:
 * clears the 100-entry glyph pool (0x1C stride, base g_pCreditsGlyphPool =
 * Credits_OverlayEnd), loads the four credits VRAM strips through
 * CdReadSyncSectors, waits on the CD stream, relocates the loaded script
 * (g_pCreditsLineTable line table, 0xC stride) and arms the scroll header
 * g_pCreditsScrollHeader.  Then, every frame: advances the header cursor and the fade
 * level g_nScreenFadeLevel, walks the visible line window
 * [g_nCreditsFirstVisibleLine .. g_nCreditsLastVisibleLine], and for each newly revealed line lays its
 * text out into free glyph records -- one record per non-space character,
 * x advanced by 0xE (0x10 for digit runs, 3/4 of a space), ':' emitting two
 * dots, and the glyph index taken from the 0x30/0x41/0x61 ranges plus a
 * small punctuation table.  A second pass re-centres the finished line on
 * (x - 0x100) / 2 and seeds each glyph's per-frame drift/colour deltas, and
 * two more passes run the per-glyph state machine (1 = fly in, 3 = fall
 * out) and trim the pool tail.  Finally the 5-way g_nCreditsMode state machine
 * pages the next credits screen in through StartCdReadAsync / CopyWords.
 *
 * ---- Why the source is shaped this way ------------------------------------
 * Each item below is load-bearing for the byte match; the matching cookbook
 * has the general form of each lever.
 *  1. The frame window sp+0x20..0x38 (x, y, colour, advance, 1, 0x1600, with
 *     a hole at 0x2C) is ONE local struct `st`: a BLKmode struct is never
 *     given hard registers, separate ints would be.
 *  2. `exit` is declared noreturn. Without the attribute its call inside the
 *     glyph loop makes every loop value call-crossing and moves them into
 *     s-registers.
 *  3. g_pLevelSampleBankEntries, g_pWorldUnpackHead and
 *     g_nLevelArchiveByteSize are volatile here: the original re-reads them
 *     after storing to them. Where it caches a global across a loop
 *     (g_nCreditsGlyphCount in the free-slot search) a plain local does that.
 *  4. Stack temp slots are reused across disjoint blocks, so the three rects
 *     sit at sp+0x18/0x20/0x28 because the first is declared at function
 *     scope and the other two share the switch's scope.
 *  5. The free-slot search tests the counter in the loop condition and the
 *     data in an inner `break`; the other order gets its exit test
 *     duplicated by loop.c.
 *  6. The free-slot and re-centre loops share one counter variable, which
 *     lowers its allocation priority onto s1 as in the original.
 *  7. `-((x - 0x100) * 3)`: written as `-(x - 0x100) * 3` combine folds the
 *     negation away.
 *  8. The glyph pool and line table are struct arrays (CredGlyph, CredLine),
 *     so the scheduler lets the scalar g_nCreditsGlyphCount load pass the
 *     field stores.
 *  9. Pool and line accesses index the struct arrays; where the original
 *     holds the pool base in a register the address is spelled
 *     `(CredGlyph *)(i * 0x1C + (int)g_pCreditsGlyphPool)`.
 * 10. Of the loops over g_nCreditsGlyphCount, only the state-machine pass is
 *     a `for`: its folded guard leaves the one 8-byte stack slot the
 *     original's frame has, and keeps g_nCreditsGlyphCount reloaded after the
 *     loop.
 * 11. `c = *s - 0x30;` then `upper = (unsigned)c < 0xA`: computing `upper`
 *     directly raises its priority over `cur`, and `c` must keep a non-byte
 *     set so the loop-top `andi c,0xff` stays.
 * 12. A call's plain `g_anCreditsPageSectors[i]` argument goes through the
 *     `sector` local so its load comes before the stack-argument store.
 * 13. The header key reads are `h + ((idx) * 3 << 3) + K`, the re-centre x
 *     is a `short gx`, g_nCdReadInFlight is volatile, and case 2's rect is
 *     written in [0..3] order.
 * 14. The CD-stream state block is one struct symbol (Cd85D8,
 *     g_pDrawBufA_s), so its block copy and field stores share a base and
 *     keep their original order without barriers.
 * 15. `g_nWorldTimAudioChunkOffset[3]` (an array reference, not a scalar) as
 *     a call argument puts its load after the fifth argument's chain.
 * 16. `e4 = g_pPathTableBuffer` right after the call, `sz` loaded before the
 *     g_nCreditsLineCount store, and `f = g_nReturningHomePhase` before the
 *     g_pLevelSampleBankEntries re-read fix three load orders.
 * 17. In the glyph loop the digit test is a block temp, `c = (st.adv * 3) /
 *     4` gives `c` its non-byte set, and the `do {} while (0)` after
 *     `upper = 1` keeps the li out of the st.adv load's delay slot.
 * 18. `i = 0;` sits above `if (g_nCreditsGlyphCount > 0)` so jump.c does not
 *     thread that branch past the first state loop.
 * 19. The 20-byte copy goes to its own symbol (g_nWorldRenderMeshChunkCount
 *     = g_nWorldCollisionTriCount) and C1C is read through the non-volatile
 *     g_nLevelArchiveChunkSize_n, which picks the movstr scratch registers
 *     a3/t0/t1/t2.
 * 20. `register char *src asm("$5")` for the e4 re-read takes away its
 *     scheduling boost, so the C1C load goes first.
 * Also: g_pDrawBufB has a second spelling (g_pDrawBufB_blk) for the setup
 * block's shared base (A240); a `do { } while (0)` sits before the 20-byte
 * struct assignment (A232); and the trim loop re-reads g_pCreditsGlyphPool
 * instead of sharing `tab` so jump.c does not cross-jump its guard (B24).
 */

#include "sample_bank.h"
#include "level_globals.h"
#include "level_funcs.h"
#include "main_shared.h"
#include "level_spawn_actor.h"
#include "credits.h"
#include "actor.h"
#include "match_asm.h"

extern void exit() __attribute__((noreturn));

typedef struct {
  int w[5];
} Blk20;

/* Credits-local spellings of main globals. The volatile ones are re-read
 * after every store (lever 3 above); the scalar / array / Blk20 views give
 * each site the address form the original uses (A240). The shared headers
 * declare these differently, so they stay here. */
extern char *volatile g_pLevelSampleBankEntries;
extern int g_nCdBaseLba[];
extern volatile int g_nCdReadInFlight[];
extern int g_nWorldTimAudioChunkOffset[];
extern volatile int g_nLevelArchiveChunkSize;
extern int g_anCameraPos;
extern short g_nCameraEulerPitch;
extern Blk20 g_nWorldCollisionTriCount;
extern Blk20 g_nWorldRenderMeshChunkCount;
extern int g_nLevelArchiveChunkSize_n;

extern char g_CutsceneSoundDef[];
extern char *g_pDrawBufA;
extern char *g_pDrawBufB;
extern char *g_pDrawBufB_blk[];
extern char *g_pPathTableBuffer;
extern volatile int g_nLevelArchiveByteSize;

typedef struct {
  char *drawBufA;                /* 0x800785D8 g_pDrawBufA */
  char *drawBufB;                /* 0x800785DC g_pDrawBufB */
  char *volatile worldUnpackHead; /* 0x800785E0 g_pWorldUnpackHead */
  char *volatile pathTableBuffer; /* 0x800785E4 g_pPathTableBuffer */
  char padE8[0x18];
  int levelArchiveBaseLba;           /* 0x80078600 g_nLevelArchiveBaseLba */
  volatile int levelArchiveByteSize; /* 0x80078604 g_nLevelArchiveByteSize */
  char pad608[0x8A40 - 0x8608];
  Blk20 blk;
} Cd85D8;
extern Cd85D8 g_pDrawBufA_s;
#define S g_pDrawBufA_s

#define POOL ((CredGlyph *)g_pCreditsGlyphPool)
#define LINES ((CredLine *)g_pCreditsLineTable)

/* A credits page's text block: byte size, the base its line pointers are
 * relative to, line count, then the CredLine table (+0xC). */
typedef struct CredText {
  int size;
  int base;
  int count;
} CredText;
/* Byte-offset read of a CredText field; a struct view reorders the loads. */
#define CRED_TEXT(hdr, f) (*(int *)((char *)(hdr) + (int)&((CredText *)0)->f))

/* The credits camera track (g_pCreditsScrollHeader): a frame clock, the
 * offset of the page's text block in g_pPathTableBuffer, the key count, then
 * one CamKey per two frames. CAM_KEY reads a field of the current key in the
 * original's byte-offset form. */
typedef struct CamKey {
  int posX, posY, posZ;
  int roll, pitch, yaw;
} CamKey;
typedef struct CamTrack {
  int clock;     /* 0x00 frames since the page started */
  int text;      /* 0x04 CredText offset in g_pPathTableBuffer */
  int keyCount;  /* 0x08 */
  CamKey key[1]; /* 0x0C one per two frames */
} CamTrack;
#define CAM_KEY_OFFSET(f) ((int)&((CamTrack *)0)->key[0].f)
#define CAM_KEY(h, f) \
  (*(int *)((char *)(h) + ((*(int *)h >> 1) * 3 << 3) + CAM_KEY_OFFSET(f)))

void Credits_Update(void) {
  short rect0[4];

  if (g_nCreditsMode == CREDITS_MODE_BUILD) {
    char *pool = Credits_OverlayEnd;
    int sector;
    if (g_pCreditsScratchBuf == 0) {
      int i;
      g_pCreditsGlyphPool = pool;
      g_nCreditsGlyphCount = 0;
      for (i = 0; i < 0x64; i++) {
        g_pCreditsGlyphPool[i * 0x1C + 4] = 0;
      }
      g_pCreditsLineTable = pool + 0xAF0;
      g_pCreditsScratchBuf = pool + 0x2800;
      g_pDrawBufA = g_pCreditsScratchBuf;
      g_pDrawBufB = g_pCreditsScratchBuf;
    }
    sector = g_anCreditsPageSectors[g_nCreditsPageIndex * 2];
    CdReadSyncSectors(g_nCdBaseLba[0], g_pDrawBufB_blk[0], 0x800, sector, 0x258);
    CopyWords(g_nWorldTimAudioChunkOffset, g_pDrawBufB_blk[0], 0x1D0);
    CdReadSyncSectors(g_nCdBaseLba[0], g_pDrawBufB_blk[0], 0x60000,
                  g_nWorldTimAudioChunkOffset[0] + g_anCreditsPageSectors[g_nCreditsPageIndex * 2], 0x258);
    rect0[0] = 0x200;
    rect0[2] = 0x200;
    rect0[1] = 0;
    rect0[3] = 0x180;
    LoadImage(rect0, g_pDrawBufB_blk[0]);
    CdReadSyncSectors(g_nCdBaseLba[0], g_pDrawBufB_blk[0], g_nWorldTimAudioChunkSize - 0x60000,
                  g_nWorldTimAudioChunkOffset[0] + g_anCreditsPageSectors[g_nCreditsPageIndex * 2] + 0x60000, 0x258);
    SetSpuTransferStartAddr(0x1010);
    WriteSpuRam(g_pDrawBufB_blk[0], 0x7EFF0);
    while (GetSpuTransferStatus(0) == 0) {
    }
    CdReadSyncSectors(g_nCdBaseLba[0], g_pDrawBufB_blk[0], g_nWorldTimAudioChunkOffset[3],
                  g_nWorldBundleChunkOffset + g_anCreditsPageSectors[g_nCreditsPageIndex * 2], 0x258);
    S.worldUnpackHead = UnpackWorldDataChunks((int *)g_pDrawBufB_blk[0], 1);
    {
      register char *src asm("$5");
      S.pathTableBuffer = S.worldUnpackHead;
      src = S.pathTableBuffer;
      g_nWorldRenderMeshChunkCount = g_nWorldCollisionTriCount;
      S.levelArchiveBaseLba = g_nLevelArchiveChunkOffset;
      S.levelArchiveByteSize = g_nLevelArchiveChunkSize_n;
      g_nPortalCount = 0;
      CdReadSyncSectors(g_nCdBaseLba[0], src, S.levelArchiveByteSize,
                    S.levelArchiveBaseLba + g_anCreditsPageSectors[g_nCreditsPageIndex * 2], 0x258);
    }
    {
    char *e4 = g_pPathTableBuffer;
    g_pLevelSampleBankEntries = g_CutsceneSoundDef;
    SAMPLE_RAW(g_pLevelSampleBankEntries, int, nSampleAddr) = 0x1010;
    SAMPLE_RAW(g_pLevelSampleBankEntries, int, nLoopAddr) = -1;
    SAMPLE_RAW(g_pLevelSampleBankEntries, short, nVolume) = 0x50;
    SAMPLE_RAW(g_pLevelSampleBankEntries, short, nPitch) = 0x659;
    SAMPLE_RAW(g_pLevelSampleBankEntries, short, nPitchSpread) = 0;
    SAMPLE_RAW(g_pLevelSampleBankEntries, short, nPitchStep) = 0;
    g_pCreditsScrollHeader = e4;
    SAMPLE_RAW(g_pLevelSampleBankEntries, int, nPitchMode) = 0;
    g_nSfxOverrideFlags = SFX_OVERRIDE_VOLUME;
    g_nSfxOverrideVolL = 0x3FFF;
    g_nSfxOverrideVolR = 0x3FFF;
    PlaySoundEffect(0, 0, SFX_MODE_PANNED, 0);
    }
    {
      char *hdr;
      char *shift;
      hdr = g_pPathTableBuffer + ((CamTrack *)g_pCreditsScrollHeader)->text;
      {
      int sz;
      g_pCreditsLoadBuf = hdr;
      sz = CRED_TEXT(hdr, size);
      shift = g_pCreditsLineTable + CRED_TEXT(hdr, base) - 0xC;
      g_nCreditsLineCount = CRED_TEXT(hdr, count);
      CopyWords(g_pCreditsLineTable, hdr + 0xC, sz - 0xC);
      }
      {
        int n = g_nCreditsLineCount;
        int i = 0;
        if (n > 0) {
          int *p = (int *)g_pCreditsLineTable;
          do {
            p[0] = (int)shift + p[0];
            i += 1;
            p += 3;
          } while (i < n);
        }
      }
    }
    g_nCreditsMode = CREDITS_MODE_FETCH_HEADER;
    g_nCreditsPageIndex += 1;
  }
  if (g_pCreditsScrollHeader != 0) {
    int gate = g_nCreditsMode;
    if (gate >= 0) {
      int *rec = (int *)g_pCreditsScrollHeader;
      int t = rec[0] + g_nFrameStep;
      int lim = rec[2] * 2;
      rec[0] = t;
      if (t < lim) {
        if (t < 0x20 || (t = lim - t, t < 0x20)) {
          g_nScreenFadeLevel = 0x10 - (t >> 1);
        } else {
          g_nScreenFadeLevel = 0;
        }
        if (g_nScreenFadeLevel >= 0x10) {
          g_nScreenFadeLevel = 0xF;
        }
        if (g_nScreenFadeLevel < 0) {
          g_nScreenFadeLevel = 0;
        }
        {
          char *h = g_pCreditsScrollHeader;
          g_anCameraPos = *(int *)((*(int *)h >> 1) * 0x18 + h + CAM_KEY_OFFSET(posX));
          g_nCameraPosY = CAM_KEY(h, posY);
          g_nCameraPosZ = CAM_KEY(h, posZ);
          g_nCameraEulerRoll = CAM_KEY(h, roll);
          g_nCameraEulerPitch = CAM_KEY(h, pitch);
          g_nCameraEulerYaw = CAM_KEY(h, yaw);
        }
      } else {
        g_nCreditsMode = gate + 1;
      }
    }
  }
  if (g_nCreditsGlyphCount == 0) {
    if (g_nCreditsFirstVisibleLine >= g_nCreditsLineCount) {
      char *p;
      SAMPLE_RAW(g_pLevelSampleBankEntries, short, nVolume) -= 0xA;
      {
        int f = g_nReturningHomePhase;
        int w;
        p = g_pLevelSampleBankEntries;
        w = SAMPLE_RAW(p, short, nVolume);
        g_nReturningHomePhase = f + g_nFrameStep;
        if (w < 0) {
          SAMPLE_RAW(p, short, nVolume) = 0;
        }
      }
      g_nScreenFadeLevel = g_nReturningHomePhase;
      if (g_nReturningHomePhase >= 0x10) {
        g_nScreenFadeLevel = 0xF;
      }
      if (g_nScreenFadeLevel == 0xF) {
        g_nCreditsMode = CREDITS_MODE_RETURN_HOME;
      }
    }
  }
  if (g_nCreditsGlyphCount > 0 || (g_nCreditsLineCount > 0 && g_nCreditsFirstVisibleLine < g_nCreditsLineCount)) {
    int last = g_nCreditsLastVisibleLine;
    int line;
    int upper;
    int i;
    RotateLightVectorXZ(3);
    line = g_nCreditsFirstVisibleLine;
    upper = 1;
    if (last >= line) {
      do {
        int cur = -1;
        if (TICK_DOWN(LINES[line].showTimer) != 0) {
          LINES[line].state = CRED_LINE_SCROLLED;
          if (LINES[line].nextDelay == 0 && line == g_nCreditsFirstVisibleLine) {
            g_nCreditsFirstVisibleLine = line + 1;
          }
        }
        if (LINES[0].state == CRED_LINE_PENDING) {
          cur = 0;
        } else if (TICK_DOWN(LINES[line].nextDelay) != 0 &&
                   LINES[line + 1].state == CRED_LINE_PENDING &&
                   line < g_nCreditsLineCount - 1) {
          cur = line + 1;
        }
        if (cur != -1) {
          int slot = 0;
          int colon = 0;
          struct {
            int x;
            int y;
            int col;
            int pad0C;
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
          LINES[cur].state = CRED_LINE_LAID_OUT;
          lift = 0x10;
          if (g_nCreditsLastVisibleLine < cur) {
            g_nCreditsLastVisibleLine = cur;
          }
          if (g_nCreditsLastVisibleLine > g_nCreditsLineCount) {
            g_nCreditsLastVisibleLine = g_nCreditsLineCount;
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
              n = g_nCreditsGlyphCount;
              i = slot;
              if (i < n) {
                while (i < n) {
                  if (POOL[i].state == CRED_GLYPH_FREE) {
                    break;
                  }
                  i += 1;
                }
                if (i < g_nCreditsGlyphCount) {
                  slot = i;
                  g = &POOL[i];
                  goto got;
                }
              }
              g = &POOL[g_nCreditsGlyphCount];
              g_nCreditsGlyphCount = g_nCreditsGlyphCount + 1;
            got:;
            }
              c = *s;
              if ((unsigned int)(c - 0x41) < 0x1A) {
                upper = 1;
              }
              if ((unsigned int)(c - 0x61) < 0x1A) {
                upper = 0;
              }
              g->state = CRED_GLYPH_FLY_IN;
              g->line = cur;
              if (g_nCreditsGlyphCount >= 0x65) {
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
              EBB_BREAK();
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
            if (g_nCreditsGlyphCount > 0) {
              do {
                CredGlyph *g = (CredGlyph *)(i * 0x1C + (int)g_pCreditsGlyphPool);
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
              } while (i < g_nCreditsGlyphCount);
            }
          }
        }
        line += 1;
      } while (last >= line);
    }
    i = 0;
    if (g_nCreditsGlyphCount > 0) {
      do {
        int st = LINES[POOL[i].line].state;
        if (st == 2) {
          if (POOL[i].state == st) {
            POOL[i].state = CRED_GLYPH_FALL_OUT;
          } else if (POOL[i].state == CRED_GLYPH_DONE) {
            POOL[i].state = CRED_GLYPH_FREE;
          }
        }
        i += 1;
      } while (i < g_nCreditsGlyphCount);
    }
    for (i = 0; i < g_nCreditsGlyphCount; i++) {
      {
        CredGlyph *g = (CredGlyph *)(i * 0x1C + (int)g_pCreditsGlyphPool);
        switch (g->state) {
        case CRED_GLYPH_FLY_IN:
          g->x =
              g->x + g->dx;
          g->r = g->r + g->dr;
          g->z =
              g->z + g->dz;
          POOL[i].g = POOL[i].g + POOL[i].dg;
          POOL[i].b = POOL[i].b + POOL[i].db;
          if ((unsigned char)(POOL[i].timer =
                                  POOL[i].timer - 1) == 0) {
            POOL[i].state = CRED_GLYPH_SETTLED;
            POOL[i].dx = RandomSignedInRange(3, 0xB);
            POOL[i].dy = -RandomIntInRange(6, 0xA);
            POOL[i].dz = RandomIntInRange(-0x23, 0x23);
            POOL[i].dr = RandomSignedInRange(4, 9);
            POOL[i].dg = RandomSignedInRange(4, 9);
            POOL[i].db = RandomSignedInRange(4, 9);
          }
          break;
        case CRED_GLYPH_FALL_OUT:
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
            POOL[i].state = CRED_GLYPH_DONE;
          }
          break;
        }
      }
    }
    if (g_nCreditsGlyphCount > 0) {
      char *tab;
      if (g_pCreditsGlyphPool[g_nCreditsGlyphCount * 0x1C - 0x18] == 0) {
        tab = g_pCreditsGlyphPool;
        for (;;) {
          int m = g_nCreditsGlyphCount - 1;
          g_nCreditsGlyphCount = m;
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
    int st = g_nCreditsMode;
    if (st != CREDITS_MODE_BUILD) {
      if (st > 0) {
        RestartStalledCdRead();
        if (g_nCdReadInFlight[0] != 0) {
          return;
        }
        if (CallCdSync(1, 0) != 2) {
          return;
        }
        st = g_nCreditsMode;
      }
      if ((unsigned int)st < 5) {
        switch (st) {
        case CREDITS_MODE_FETCH_HEADER: {
          int sector = g_anCreditsPageSectors[g_nCreditsPageIndex * 2];
          StartCdReadAsync(g_nCdBaseLba[0], g_pCreditsLoadBuf, 0x800, sector, 0x258);
          g_nCreditsMode = g_nCreditsMode + 1;
          break;
        }
        case CREDITS_MODE_FETCH_TEXTURES:
          CopyWords(g_nWorldTimAudioChunkOffset, g_pCreditsLoadBuf, 0x1D0);
          StartCdReadAsync(g_nCdBaseLba[0], g_pCreditsLoadBuf, 0x20000,
                        g_nWorldTimAudioChunkOffset[0] + g_anCreditsPageSectors[g_nCreditsPageIndex * 2], 0x258);
          g_nCreditsMode = g_nCreditsMode + 1;
          break;
        case CREDITS_MODE_FETCH_SCENE: {
          short rect[4];
          int span;
          rect[0] = 0x200;
          rect[1] = 0x180;
          rect[2] = 0x200;
          rect[3] = 0x80;
          DrawSync(0);
          LoadImage(rect, g_pCreditsLoadBuf);
          DrawSync(0);
          {
            int off = g_nLevelArchiveChunkSize + 0x40000;
            span = g_nWorldBundleChunkSize + off;
          }
          g_nCreditsLoadSpan = span;
          StartCdReadAsync(g_nCdBaseLba[0], g_pCreditsLoadBuf, span,
                        g_nWorldTimAudioChunkOffset[0] + g_anCreditsPageSectors[g_nCreditsPageIndex * 2] + 0x20000,
                        0x258);
          g_nCreditsMode = g_nCreditsMode + 1;
          break;
        }
        case CREDITS_MODE_WAIT_SCROLL:
          g_nEnteredPortalIdx = 0;
          break;
        case CREDITS_MODE_SWAP_SCENE: {
          short rect[4];
          rect[0] = 0x200;
          rect[1] = 0x80;
          rect[2] = 0x200;
          rect[3] = 0x100;
          g_pDrawBufA = g_pCreditsScratchBuf;
          g_pDrawBufB = g_pCreditsScratchBuf;
          DrawSync(0);
          LoadImage(rect, g_pCreditsLoadBuf);
          rect[0] = 0x200;
          rect[1] = 0x180;
          rect[2] = 0x200;
          rect[3] = 0x80;
          MoveImage(rect, 0x200, 0);
          DrawSync(0);
          CopyWords(g_pCreditsScratchBuf, g_pCreditsLoadBuf + 0x40000, g_nCreditsLoadSpan - 0x40000);
          g_pCreditsLoadBuf = g_pCreditsScratchBuf + g_nCreditsLoadSpan - 0x40000;
          S.worldUnpackHead = UnpackWorldDataChunks((int *)g_pDrawBufB, 1);
          {
          char *dc = S.drawBufB;
          S.blk = g_nWorldCollisionTriCount;
          g_pPathTableBuffer = dc + g_nWorldBundleChunkSize;
          }
          g_pCreditsScrollHeader = g_pPathTableBuffer;
          g_nLevelArchiveByteSize = g_nLevelArchiveChunkSize;
          g_nLevelArchiveBaseLba = g_nLevelArchiveChunkOffset;
          if (g_nCreditsPageIndex % 10 < 9) {
            g_nCreditsMode = CREDITS_MODE_FETCH_HEADER;
          } else {
            g_nCreditsMode = CREDITS_MODE_WAIT_SCROLL;
          }
          g_nCreditsPageIndex += 1;
          break;
        }
        }
      }
    }
  }
}
