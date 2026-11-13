#include "actor.h"
#include "actor_states.h"
#include "actor_type.h"
#include "angle.h"
#include "funcs.h"
#include "globals.h"
#include "gpu_prim.h"
#include "level_id.h"
#include "main_shared.h"
#include "vecmath.h"

/* The queue tail is re-read from the global at every field access. */
#define CURSOR (*(Actor *volatile *)&g_pSpriteRecordWriteCursor)
#define CURSOR_P g_pSpriteRecordWriteCursor

/* Emit a glyph-sprite chain for a string: per non-space byte allocate a
   0x58-byte SPRT slot at g_pSpriteRecordWriteCursor (queue tail, grows down),
   map '0'-'9' and 'A'-'Z' (+ the specials '/', '?', '%', '^', '+') to font
   tile indices, copy the pen position from *pen and advance it by nAdvance
   between glyphs (0x80017fe4, 0x1c8).

   Load-bearing, do not "clean" any of it up:
     - CURSOR is the volatile-cast macro (14 separate lui/lw reloads) and
       CURSOR_P is the same address read PLAINLY. Which one a site uses is a
       scheduling decision, not a style choice: a volatile read is pinned in
       sched2's order, so it emits its two-instruction symbol load at the head
       of the block, and reorg's fill_slots_from_thread stops there because an
       8-byte insn is not delay-slot eligible. The four plain sites are exactly
       the ones whose value insn has to reach a delay slot -- the allocation
       write-back (so `li a2,0x58` sinks into the `jal FillWord` slot instead of
       filling the load-delay hole), and the digit / letter / default arms (so
       each arm's `addiu v0,a0,K` leads its thread and lands in the preceding
       branch's slot, which also stops jump.c from cross-jumping those three
       `sh`s into the shared tail). The six equality arms keep the volatile
       read and DO get merged, which is what the original has.
     - the FillWord argument is the LOCAL `rec`, not a reload, so the
       allocation is `rec = CURSOR - 1; CURSOR_P = rec;` as two
   statements; the embedded assignment form routes the value through v0 (+3
   insns).
     - the range tests are on an `int ch = *pszText;` (a0, unmasked) and the six
       equality tests on `(unsigned char)ch`, which is what emits the single
       `andi v1,a0,0xFF` shared by the whole ladder.
     - per-arm stores, no tile local: a shared `int tile` (function-scope) costs
       a register (+10 words), and block-scoped per-arm temps are byte-identical
       to the plain form. */
void *BuildTextSpriteChain(unsigned char *pszText, int *pPen, int nAdvance,
                           int nPalette) {
  Actor *rec;
  int ch;

  while (*pszText != 0) {
    if (*pszText != ' ') {
      rec = CURSOR - 1;
      CURSOR_P = rec;
      FillWord(rec, 0, ACTOR_SIZE);
      CopyVector(&CURSOR->posX, pPen);

      ch = *pszText;
      if (ch >= '0' && ch <= '9') {
        CURSOR_P->type = ACTOR_TYPE_DIGIT_GLYPH(ch);
      } else if (ch >= 'A' && ch <= 'Z') {
        CURSOR_P->type = ACTOR_TYPE_LETTER_GLYPH(ch);
      } else if ((unsigned char)ch == '/') {
        CURSOR->type = ACTOR_TYPE_GLYPH_SLASH;
      } else if ((unsigned char)ch == '?') {
        CURSOR->type = ACTOR_TYPE_GLYPH_QUESTION;
      } else if ((unsigned char)ch == '%') {
        CURSOR->type = ACTOR_TYPE_GLYPH_PERCENT;
      } else if ((unsigned char)ch == '^') {
        CURSOR->type = ACTOR_TYPE_GLYPH_CARET;
      } else if ((unsigned char)ch == '+') {
        CURSOR->type = ACTOR_TYPE_GLYPH_PLUS;
      } else {
        CURSOR_P->type = ACTOR_TYPE_GLYPH_PERIOD;
      }

      CURSOR->otBias = OT_BIAS_SPRITE;
      CURSOR->tintMode = nPalette;
      CURSOR->cullRadius = ACTOR_CULL_ALWAYS;
    }
    *pPen += nAdvance;
    pszText++;
  }
  return CURSOR;
}

#undef CURSOR
#undef CURSOR_P

/* The sprite-record cursor as an Actor, read through a volatile view or
   plainly (see note 3 below). */
#define CURSOR_V (*(Actor *volatile *)&g_pSpriteRecordWriteCursor)

/* Emit a glyph-sprite chain for a UI string, tracking per-glyph kerning: per
   non-space byte allocate a 0x58-byte SPRT slot at g_pSpriteRecordWriteCursor
   (queue tail, grows down), copy the pen from *pos, map the byte to a font
   tile ('0'-'9', 'A'-'Z', and specials '!',',','?','.') and advance the pen by
   the digit-advance spacing after digits / '!'/'?', else by the glyph width
   attr[0]. Spaces advance the pen by 3/4 of the glyph width.
   (0x800181ac, 0x2a0 bytes.) */
/* Three things in the body below are load-bearing; do not "clean" them up.
   1. The do/while(0) nests are a ref-count dial (A238). global.c weights
      REG_N_REFS by loop depth, and the three long-lived pseudos here race for
      s0: flag 15 refs/109 insns (1.65) beats text 14/118 (1.42) beats
      pos 13/118 (1.32), where the original wants pos -> s0,
      text -> s1, flag -> s2. The 2-deep nest on `text += 1;` takes
      text to 16 refs and the 6-deep nest on `*pos = adv;` takes pos
      to 19; 5 is not enough (the pair ties and the lower allocno, text,
      wins).
   2. The pen advance is stored once PER ARM through a block-scoped `adv`
      (A201). A shared carrier is a global allocno and can only reach v1; the
      per-arm temps are local qtys and take v0, and jump.c cross-jumps the two
      identical stores back into one.
   3. Only SOME of the cursor reads are volatile, and which ones is not
      cosmetic. A volatile read is pinned in sched2's order, so a block whose
      cursor read is volatile emits the (two-instruction, delay-slot-ineligible)
      symbol load FIRST and reorg's fill_slots_from_thread stops there. That is
      why the letter arm reads the cursor plainly -- its `addiu v0,ch,0x169` has
      to be the first insn of the fall-through thread for the `beqz`'s delay
      slot -- and why the allocation store is written back plainly, which lets
      `li a2,0x58` sink to the `jal FillWord` delay slot instead of filling the
      load-delay hole. */

void *BuildTextSprites(char *text, int *pos, int *attr, int spacing,
                       int flags) {
  unsigned char c;
  int w3;
  int ch;
  int flag;
  c = *text;
  flag = 1;
  while (c != 0) {
    if ((c & 0xFF) != ' ') {
      Actor *rec = CURSOR_V - 1;
      g_pSpriteRecordWriteCursor = rec;
      FillWord(rec, 0, ACTOR_SIZE);
      CopyVector(&CURSOR_V->posX, pos);
      c = *text;
      if ((c == '!') || (c == '?')) {
        flag = 1;
      }
      if (flag == 0) {
        Actor *r = CURSOR_V;
        r->posY = (r->posY) + attr[1];
        r->posZ = attr[2];
      }
      ch = *text;
      if (((unsigned int)(ch - '0')) < 10) {
        g_pSpriteRecordWriteCursor->type = ACTOR_TYPE_DIGIT_GLYPH(ch);
      } else if (((unsigned int)(ch - 'A')) < 26) {
        g_pSpriteRecordWriteCursor->type = ACTOR_TYPE_LETTER_GLYPH(ch);
      } else if ((ch & 0xFF) == '!') {
        CURSOR_V->type = ACTOR_TYPE_GLYPH_BANG;
      } else if ((ch & 0xFF) == ',') {
        CURSOR_V->type = ACTOR_TYPE_GLYPH_COMMA;
      } else if ((ch & 0xFF) == '?') {
        CURSOR_V->type = ACTOR_TYPE_GLYPH_QUESTION;
      } else if ((ch & 0xFF) == '.') {
        CURSOR_V->type = ACTOR_TYPE_GLYPH_PERIOD;
      } else {
        Actor *r = CURSOR_V;
        r->type = ACTOR_TYPE_GLYPH_COMMA;
        r->posY = (r->posY) - ((attr[0] * 2) / 3);
      }
      CURSOR_V->otBias = OT_BIAS_SPRITE;
      CURSOR_V->tintMode = flags;
      CURSOR_V->cullRadius = ACTOR_CULL_ALWAYS;
      if (flag != 0) {
        int adv = (*pos) + spacing;
        do {
          do {
            do {
              do {
                do {
                  do {
                    *pos = adv;
                  } while (0);
                } while (0);
              } while (0);
            } while (0);
          } while (0);
        } while (0);
      } else {
        int adv = (*pos) + attr[0];
        do {
          do {
            do {
              do {
                do {
                  do {
                    *pos = adv;
                  } while (0);
                } while (0);
              } while (0);
            } while (0);
          } while (0);
        } while (0);
      }
      {
        int t = (*text) - '0';
        flag = ((unsigned int)t) < 10;
      }
    } else {
      w3 = attr[0] * 3;
      flag = 1;
      if (w3 < 0) {
        w3 += 3;
      }
      *pos += w3 >> 2;
    }
    do {
      do {
        text += 1;
      } while (0);
    } while (0);
    c = *text;
  }

  return *((void *volatile *)(&g_pSpriteRecordWriteCursor));
}

#undef CURSOR_V

/* 0x8001844c (0xE8) - build a LINE_G2 prim between two screen points,
   shading each endpoint by its angular distance from the light-sweep
   angle (radial highlight), then queue it and bump the prim cursor.

   Both shades come off the 0x80 full-brightness base, and the far endpoint's
   shade must be a FRESH local: local-alloc ties a destination to a source that
   dies in the same insn, so `far = full - AbsAngleDelta8(...)` keeps the
   subtract in place in the base's register AND settles the whole-function
   callee-saved roles. Reusing a parameter for it costs the role rotation, and
   a self-decrement (`full = full - ...`) makes `full` a two-death quantity
   that local-alloc drops, so global-alloc puts it on a fifth callee-saved and
   the function overflows its slot. */
void EmitRadialShadedLine(int x0, int y0, int x1, int y1) {
  LineG2 *prim;
  int shade0;
  int shade;
  int full;
  int shade1;
  int dx;
  int dy;

  prim = g_pPrimBufferWriteCursor;
  prim->tag = PRIM_TAG(4);
  prim->code = GPU_CODE_LINE_G2;
  prim->x0 = x0;
  prim->y0 = y0;
  prim->x1 = x1;
  prim->y1 = y1;

  shade = x0 - 0x100;
  shade0 = AbsAngleDelta8(ArcTan2_8bit(shade, y0 - 0x78),
                          g_anLightSweepAngleBlock[0]);
  dx = x1 - 0x100;
  dy = y1 - 0x78;
  full = 0x80;
  shade0 = full - shade0;
  prim->r0 = shade0 + 0x60;
  prim->g0 = shade0 + 0x60;
  prim->b0 = shade0;

  shade1 =
      full - AbsAngleDelta8(ArcTan2_8bit(dx, dy), g_anLightSweepAngleBlock[0]);
  shade = shade1 + 0x60;
  prim->r1 = shade;
  prim->g1 = shade;
  prim->b1 = shade1;

  AddPrimToOT((int)prim);
  g_pPrimBufferWriteCursor = prim + 1;
}

/* Push a blinking SPRT_16 marker onto the front of the sprite queue
   (0x80018534): g_pSpriteRecordWriteCursor bumps down 0x58 bytes. Gated by
   (frame & 0x1f) < 0x10 so it shows for half of each 32-frame cycle;
   tile < 2 sets rotX = 0x40 and rotZ = tile * 0x80. The marker is the '1'
   digit glyph model (text glyphs are `ch + 0xD4`, BuildTextSprites), turned
   by those angles. */
void EnqueueBlinkingMarkerSprite(int *pos, int frame, int tile) {
  int *dst;

  if ((frame & 0x1f) < 0x10) {
    g_pSpriteRecordWriteCursor--;
    FillWord(g_pSpriteRecordWriteCursor, 0, ACTOR_SIZE);
    dst = &g_pSpriteRecordWriteCursor->posX;
    g_pSpriteRecordWriteCursor->type = ACTOR_TYPE_GEM_DIGIT_0 + 1;
    CopyVector(dst, pos);
    if (tile < 2) {
      g_pSpriteRecordWriteCursor->rotX = 0x40;
      g_pSpriteRecordWriteCursor->rotZ = tile << 7;
    }
    g_pSpriteRecordWriteCursor->otBias = OT_BIAS_SPRITE;
    g_pSpriteRecordWriteCursor->tintMode = TINT_HUD;
    g_pSpriteRecordWriteCursor->cullRadius = ACTOR_CULL_ALWAYS;
  }
}

/* Draw a menu rectangle (0x8001860c): semi-transparent POLY_F4 fill from
   (left,top) to (right,bottom) plus four EmitRadialShadedLine
   borders. Called by SaveLoadMenu_Draw. */
void DrawShadedMenuBox(int left, int right, int top, int bottom) {
  ModePolyF4 *prim;

  prim = g_pPrimBufferWriteCursor;
  SetDrawMode(g_pPrimBufferWriteCursor, 1, 0, 0x40, 0);
  AddPrimToOT((int)prim);
  prim->poly.tag = PRIM_TAG(5);
  prim->poly.code = (GPU_CODE_POLY_F4 | GPU_SEMI_TRANS);
  prim->poly.x0 = left;
  prim->poly.x1 = right;
  prim->poly.x2 = left;
  prim->poly.x3 = right;
  prim->poly.y0 = top;
  prim->poly.y1 = top;
  prim->poly.y2 = bottom;
  prim->poly.y3 = bottom;
  prim->poly.r0 = 0x70;
  prim->poly.g0 = 0x70;
  prim->poly.b0 = 0x70;
  AddPrimToOT((int)&prim->poly);
  g_pPrimBufferWriteCursor = prim + 1;
  EmitRadialShadedLine(left, top, right, top);
  EmitRadialShadedLine(right, top, right, bottom);
  EmitRadialShadedLine(right, bottom, left, bottom);
  EmitRadialShadedLine(left, bottom, left, top);
}

/* 0x80018728 (0x150) — draw the dragon-rescue banner: "RESCUED" plus the
   freed dragon's name (g_apDragonNames[] at the statue's
   DragonStatueState.dragonName, +0x38) as two BuildTextSpriteChain labels
   right-shifted by the name length, then walk the sprite records the two calls
   appended (queue grows downward) and rock each glyph: its rotZ, the roll
   angle RasterizeSpritePrimQueue builds the sprite's rotation from, follows
   the cosine LUT off the dragon-rescue timer, phase-stepped 0xC per glyph.
 */
void DrawDragonRescuedBanner(void) {
  int pos[3];
  char *name;
  Actor *rec;
  int w;
  int phase;

  name =
      g_apDragonNames[((DragonStatueState *)g_pDragonRescueSourceActor->state)
                          ->dragonName];
  w = (strlen(name) - 1) * 13;
  rec = g_pSpriteRecordWriteCursor;

  pos[0] = 0xB0 - w;
  pos[1] = 0xC8;
  pos[2] = 0x1000;
  BuildTextSpriteChain(g_szUiRescued, pos, 0x14, TINT_GOLD);

  pos[0] = 0x150 - w;
  pos[1] = 0xC8;
  pos[2] = TEXT_TITLE_DEPTH;
  BuildTextSpriteChain(name, pos, 0x1A, TINT_GOLD);

  phase = 0;
  rec--;
  if ((int)rec >= (int)g_pSpriteRecordWriteCursor) {
    int *timer = &g_nDragonRescueSecondaryTimer;
    unsigned short *lut = (unsigned short *)g_anSineLut + 64;
    do {
      int idx = (*timer * 2 + phase) & 0xFF;
      rec->rotZ = *(unsigned short *)((idx << 1) + (int)lut) >> 7;
      rec--;
      phase += 0xC;
    } while ((int)rec >= (int)g_pSpriteRecordWriteCursor);
  }
}

/* Empty stub at 0x80018878 — bare `jr $ra; nop`.
   Unreferenced: no call, branch, pointer or table entry in main or any overlay
   (xref). */
void EmptyStub(void) {}

/* Refill the null-terminated sprite/HUD primitive request queue (0x80018880):
   append each remaining 0x58-byte primitive record (from
   g_pSpriteRecordWriteCursor up to g_pSpriteRecordBufferTop) as a queue entry
   after the last live one, then re-terminate with NULL. Called near the end of
   GamestateDraw. */
void EnqueuePendingSpritePrims(void) {
  void **p;

  p = g_apSpritePrimQueue;
  if (g_apSpritePrimQueue[0] != 0) {
    p = g_apSpritePrimQueue;
    do {
      p = p + 1;
    } while (*p != 0);
  }
  if (g_pSpriteRecordWriteCursor != g_pSpriteRecordBufferTop) {
    do {
      *p = g_pSpriteRecordWriteCursor;
      g_pSpriteRecordWriteCursor++;
      p = p + 1;
    } while (g_pSpriteRecordWriteCursor != g_pSpriteRecordBufferTop);
  }
  *p = 0;
}

/* Build the "DEMO MODE" text overlay (0x80018908) via BuildTextSprites, then
   rock each glyph (its rotZ roll angle) on the cosine LUT indexed by the
   vblank tick, and hand the records to the sprite queue via
   EnqueuePendingSpritePrims. */
void DrawDemoModeOverlay(void) {
  Actor *first;
  Actor *glyph;
  int phase;
  int pos[4];
  int size[4];

  first = g_pSpriteRecordWriteCursor;
  size[0] = 0x10;
  size[1] = 1;
  size[2] = TEXT_SMALL_DEPTH;
  pos[0] = 199;
  pos[1] = 200;
  pos[2] = TEXT_DEPTH;
  BuildTextSprites(g_szDemoMode, pos, size, 0x12, TINT_GOLD);
  glyph = first - 1;
  phase = 0;
  if ((int)g_pSpriteRecordWriteCursor <= (int)glyph) {
    do {
      glyph->rotZ =
          ((unsigned short *)
               g_anSineLut)[((g_nVblankTickCount * 4 + phase) & 0xff) + 0x40] >>
          7;
      glyph--;
      phase += 0xc;
    } while ((int)g_pSpriteRecordWriteCursor <= (int)glyph);
  }
  EnqueuePendingSpritePrims();
}

/* 0x800189f0 — the motion-trail ribbons (1344 b), drawn from ComposeFrameScene.

   Up to three ribbons are queued per frame (see the level-33 pusher): each
   entry is a point list of 0x1c-byte slices plus its slice count.  Every slice
   is projected to screen space, then a shaded quad is emitted between each
   consecutive pair: the rim offset is the screen-space perpendicular of the
   slice-to-slice delta, scaled by the ribbon half-width over the slice depth,
   and the vertex colours fade out with the slice age.  The half-width ramps in
   over the first three slices and collapses to 0 on the last-but-one; the
   Gnasty Gnorc fight (LEVEL_GNASTY_GNORC) quarters it and swaps red with green.
 */

typedef struct {  /* 0x1c */
  int wx, wy, wz; /* 0x00 world position */
  int sx, sy;     /* 0x0c projected screen position */
  int z;          /* 0x14 projected depth */
  int age;        /* 0x18 slice age (fade + cull) */
} TrailSlice;

typedef struct {                  /* 0x30: DR_MODE + POLY_G4 */
  int mode[3];                    /* 0x00 */
  int tag;                        /* 0x0c */
  unsigned char r0, g0, b0, code; /* 0x10 */
  short x0, y0;                   /* 0x14 */
  unsigned char r1, g1, b1, p1;   /* 0x18 */
  short x1, y1;                   /* 0x1c */
  unsigned char r2, g2, b2, p2;   /* 0x20 */
  short x2, y2;                   /* 0x24 */
  unsigned char r3, g3, b3, p3;   /* 0x28 */
  short x3, y3;                   /* 0x2c */
} TrailQuad;

void DrawMotionTrailRibbons(void) {
  int d[3];
  int i;
  int x0, x1, y0, y1;
  int px0, px1, py0, py1;
  int drawn;
  TrailSlice *pts;
  int k;
  int wide;
  int dx, dy;
  int len;
  int slot;
  int r0, g0, b0, r1, g1, b1;
  int hold;
  TrailQuad *prim;

  drawn = 0;
  for (i = 0; i < g_nMotionTrailRibbonCount; i++) {
    pts = (TrailSlice *)g_apMotionTrailSlices[i];
    for (k = 0; k < g_anMotionTrailSliceCounts[i]; k++) {
      ProjectWorldPointGTE(&pts[k].sx, &pts[k].wx, 1);
    }
    for (k = 0; k < g_anMotionTrailSliceCounts[i] - 1; k++) {
      wide = 0x11170;
      wide = -(k != g_anMotionTrailSliceCounts[i] - 2) & wide;
      if (k < 3) {
        wide = (k * 0x271) << 5;
      }
      if (g_nActiveLevelId == LEVEL_GNASTY_GNORC) {
        wide >>= 2;
      }
      if (pts[k].age < 0x51) {
        SubtractVector(d, &pts[k].sx, &pts[k + 1].sx);
        dx = d[0];
        dy = d[1];
        d[0] = dy;
        d[1] = -dx;
        drawn++;
        len = d[0] * d[0] + d[1] * d[1];
        len = IntegerSqrt(len);
        if (len == 0 || pts[k].z == 0) {
          d[0] = 0;
          d[1] = 0;
        } else {
          d[0] = d[0] * wide / (pts[k].z * len);
          d[1] = d[1] * wide / (pts[k].z * len);
        }
        x0 = pts[k].sx + d[0];
        x1 = pts[k].sx - d[0];
        y0 = pts[k].sy + d[1];
        y1 = pts[k].sy - d[1];
        if (drawn >= 2) {
          slot = pts[k].z >> 7;
          if ((unsigned int)slot < 0x7D0) {
            r0 = 0x96 - pts[k].age * 2;
            g0 = 0x96 - pts[k].age * 4;
            b0 = 0x96 - pts[k].age * 10;
            r1 = 0x96 - pts[k - 1].age * 2;
            g1 = 0x96 - pts[k - 1].age * 4;
            b1 = 0x96 - pts[k - 1].age * 10;
            if (r0 < 0) {
              r0 = 0;
            }
            if (g0 < 0) {
              g0 = 0;
            }
            if (b0 < 0) {
              b0 = 0;
            }
            if (r1 < 0) {
              r1 = 0;
            }
            if (g1 < 0) {
              g1 = 0;
            }
            if (b1 < 0) {
              b1 = 0;
            }
            if (g_nActiveLevelId == LEVEL_GNASTY_GNORC) {
              hold = r0;
              r0 = g0;
              g0 = hold;
              hold = r1;
              r1 = g1;
              g1 = hold;
            }
            prim = (TrailQuad *)g_pPrimBufferWriteCursor;
            SetDrawMode(prim, 1, 0, 0x20, 0);
            AddPrimToOTSlot(prim, slot);
            prim->tag = PRIM_TAG(8);
            prim->code = (GPU_CODE_POLY_G4 | GPU_SEMI_TRANS);
            prim->x0 = x0;
            prim->x1 = x1;
            prim->x2 = px0;
            prim->x3 = px1;
            prim->y0 = y0;
            prim->y1 = y1;
            prim->y2 = py0;
            prim->y3 = py1;
            prim->r0 = r0;
            prim->g0 = g0;
            prim->b0 = b0;
            prim->r1 = r0;
            prim->g1 = g0;
            prim->b1 = b0;
            prim->r2 = r1;
            prim->g2 = g1;
            prim->b2 = b1;
            prim->r3 = r1;
            prim->g3 = g1;
            prim->b3 = b1;
            AddPrimToOTSlot(&prim->tag, slot);
            g_pPrimBufferWriteCursor = prim + 1;
          }
        }
        px0 = x0;
        px1 = x1;
        py0 = y0;
        py1 = y1;
      }
    }
  }
  g_nMotionTrailRibbonCount = 0;
}

/* Draw two stacked black POLY_F4 cinematic bars (top y=0..h, bottom
   y=240-h..240) where h = g_nLetterboxBarHeight animates 0..0x16 driven by
   g_nCinematicActive — the "letterbox close"/"open" cutscene transition.
   Submits both bars via AddPrimToOT. (0x80018f30, 0x1a4 bytes) */
void DrawCinematicLetterbox(void) {
  PolyF4 *q;
  int h;
  int h2;
  int tag;
  int code;
  if (g_nCinematicActive != 0) {
    if (g_nLetterboxBarHeight < 0x16) {
      g_nLetterboxBarHeight += g_nFrameStep;
    }
    if (g_nLetterboxBarHeight > 0x16) {
      g_nLetterboxBarHeight = 0x16;
    }
    tag = PRIM_TAG(5);
    code = GPU_CODE_POLY_F4;
  } else {
    if (g_nLetterboxBarHeight > 0) {
      g_nLetterboxBarHeight -= g_nFrameStep;
    }
    if (g_nLetterboxBarHeight < 0) {
      g_nLetterboxBarHeight = 0;
    }
    tag = PRIM_TAG(5);
    code = GPU_CODE_POLY_F4;
  }
  q = g_pPrimBufferWriteCursor;
  q->code = code;
  h = g_nLetterboxBarHeight;
  q->tag = tag;
  q->x0 = 0;
  q->x1 = SCREEN_W;
  q->x2 = 0;
  q->x3 = SCREEN_W;
  q->y0 = 0;
  q->y1 = 0;
  q->r0 = 0;
  q->g0 = 0;
  q->b0 = 0;
  q->y2 = h;
  q->y3 = h;
  AddPrimToOT((int)q);
  q++;
  g_pPrimBufferWriteCursor = q;
  q->code = code;
  h2 = g_nLetterboxBarHeight;
  q->y2 = SCREEN_H;
  q->y3 = SCREEN_H;
  q->tag = tag;
  q->x0 = 0;
  q->x1 = SCREEN_W;
  q->x2 = 0;
  q->x3 = SCREEN_W;
  q->r0 = 0;
  q->g0 = 0;
  q->b0 = 0;
  q->y0 = SCREEN_H - h2;
  q->y1 = SCREEN_H - h2;
  AddPrimToOT((int)q);
  q++;
  g_pPrimBufferWriteCursor = q;
}

/* MATCHED 2026-08-14.

   The last 2-insn residue was a coupled pair: the two shared header constants
   (0x5000000, 0x28) are DEFINED in the clamp-exit blocks in first-use order,
   while the two header STORES emit in source order — and the original needs
   those two orders opposite ([tag, code] defs, [code, tag] stores). Writing the
   constants as literals makes both orders one source decision, so either the
   stores or the constants came out reversed.

   The decoupling lever: give each constant a local that is ASSIGNED IN EVERY
   ARM of the preceding if/else. Two reaching definitions means cse cannot
   const-propagate the value to its use and re-create the def there (which is
   what defeated the earlier single-assignment `code`/`cb` locals), so the def
   stays where the source puts it — at the end of the clamp arms, in written
   order — and jump.c/reorg then duplicate it into the clamp-exit delay slots
   exactly as the original does. The store order is then free to be written
   independently.

   Also load-bearing, from the earlier passes:
     * `h` must be TWO locals, one per bar. The original reads the bar height
       into v0 in the top-bar block and v1 in the bottom one, which one shared
       multi-set local can never produce (one pseudo = one quantity); with a
       local per block each read is single-set and each gets its own register.
     * the top bar's tag store and the 0x28 store are separated by the height
       load, and a load cannot cross either store (both are MEMs off the same
       base, no alias info), so the two stores land in slots 1 and 4 of the
       block in SOURCE order. */

/* Submit a semi-transparent fullscreen POLY_F4 (rect (0,8)-(512,232)) of color
   (r,g,b), blend mode `abr` (tpage bits 5..6), into the active OT slot
   (0x800190d4). Allocates from the primitive buffer and chains via AddPrimToOT.
   Used for fade/tint overlays. */
void DrawFullscreenTint(int abr, int r, int g, int b) {
  ModePolyF4 *prim;

  prim = g_pPrimBufferWriteCursor;
  SetDrawMode(g_pPrimBufferWriteCursor, 1, 0, abr << 5, 0);
  AddPrimToOT((int)prim);
  prim->poly.tag = PRIM_TAG(5);
  prim->poly.code = (GPU_CODE_POLY_F4 | GPU_SEMI_TRANS);
  prim->poly.y0 = DRAW_Y;
  prim->poly.y1 = DRAW_Y;
  prim->poly.x0 = 0;
  prim->poly.x1 = SCREEN_W;
  prim->poly.x2 = 0;
  prim->poly.y2 = DRAW_Y + DRAW_H;
  prim->poly.x3 = SCREEN_W;
  prim->poly.y3 = DRAW_Y + DRAW_H;
  prim->poly.r0 = r;
  prim->poly.g0 = g;
  prim->poly.b0 = b;
  AddPrimToOT((int)&prim->poly);
  g_pPrimBufferWriteCursor = prim + 1;
}

/* Emit a textured POLY_FT4 quad (0x8001919c): rect (x,y,w,h) = rect[0..3],
   UV+CLUT and UV+tpage words = uv[0..1], RGB = rgb[0..2] (or 0x80 grey when
   rgb is NULL). The other three corners' UVs are derived from u0/v0 plus the
   rect size. Chains via AddPrimToOT and advances the primitive buffer by
   0x28. */
void EmitTexturedQuad(short *rect, unsigned int *uv, unsigned int *rgb) {
  PolyFT4 *q;

  q = g_pPrimBufferWriteCursor;
  q->tag = PRIM_TAG(9);
  q->code = GPU_CODE_POLY_FT4;
  if (rgb == 0) {
    q->r0 = 0x80;
    q->g0 = 0x80;
    q->b0 = 0x80;
  } else {
    q->r0 = rgb[0];
    q->g0 = rgb[1];
    q->b0 = rgb[2];
  }
  q->x0 = rect[0];
  q->y0 = rect[1];
  q->x1 = rect[0] + rect[2];
  q->y1 = rect[1];
  q->x2 = rect[0];
  q->y2 = rect[1] + rect[3];
  q->x3 = rect[0] + rect[2];
  q->y3 = rect[1] + rect[3];
  *(int *)&q->u0 = uv[0];
  *(int *)&q->u1 = uv[1];
  q->u1 = q->u0 + rect[2];
  q->u2 = q->u0;
  q->u3 = q->u1;
  q->v2 = q->v0 + rect[3];
  q->v3 = q->v0 + rect[3];
  AddPrimToOT((int)q);
  g_pPrimBufferWriteCursor = q + 1;
}

/* g_anLevelSpriteTexWordPairs (0x8007850c), reached off the held HUD block
 * base the butterfly quads use. */
#define LEVEL_SPRITE_TEX_VIA_HUD                                               \
  ((unsigned int *)(g_abHudButterflyRingBlock + 0x562))

/* Rebuilds the in-level HUD sprite-record queue (0x80019300, 0x398); also
   drawn under the pause and inventory menus. Appends, to the first free slot of
   g_apSpritePrimQueue: the five gem-counter icon records (starting at record
   4 when the counter is in state 4), the gem digit chain built on the fly by
   sprintf + BuildTextSpriteChain (each digit record rocks on a cosine wave
   written to its rotZ), then three dragon records, three life records and the
   level-egg record, terminated with a NULL slot. Skipped wholesale on flight
   levels.

   Always (flight levels included) the sparx-food butterflies and the egg
   icons are emitted directly as textured quads: the butterflies get a swept
   cosine shade (one 1/20th-turn phase step per butterfly), the eggs cycle
   through nine UV pairs.

   The roll timer and both quad loops address their blocks through the
   global symbol, not the flag pointer: cse rewrites each constant address
   relative to whichever register already holds a related one, which is what
   puts the original's held bases (and loop.c's hoist order) in place. */

/* Distinct incomplete-array views of the five g_abHudCounterDisplayState
   bytes: each block loads its own flag at [0] and derives its record address
   off the same base. The HUD words below are read off those same bases;
   naming the scalar globals instead re-materializes their addresses. */
#define GEM_HOLD_TIMER                                                         \
  (*(int *)&g_abHudGemRollBlock[0xC]) /* g_nHudLevelGemHoldTimer */
#define BUTTERFLY_COUNT                                                        \
  (*(int *)&g_abHudButterflyRingBlock[0x36]) /* g_nHudButterflyCachedCount */
#define EGG_COUNT                                                              \
  (*(int *)&g_abHudEggIconBlock[0x29]) /* g_nHudEggCachedCount */
#define FRAME_CYCLE                                                            \
  (*(int *)&g_abHudEggIconBlock[0x3D]) /* g_nHudFrameCycleMod9 */

void EnqueueHudSprites(void) {
  void **slot;
  byte *gems;
  byte *digits;
  Actor *rec;
  byte *dragons;
  byte *lives;
  byte *eggs;
  byte *ring;
  byte *quad;
  byte *uvFrame; /* the egg icon's animation frame UVs */
  int i;
  int phase;
  int shade;
  int step;
  int scratch[4]; /* sprintf text buffer / EmitTexturedQuad colour triple */
  int rect[3];
  int spare[4];

  if (g_nFlightLevelActive == 0) {
    slot = g_apSpritePrimQueue;
    if (*slot != 0) {
      slot += 1;
      while (*slot != 0) {
        slot += 1;
      }
    }
    gems = g_abHudGemCounterBlock;
    if (gems[0] != 0) {
      i = (gems[0] == 4) * 4;
      gems += 0x44;
      if (i < 5) {
        gems += i * ACTOR_SIZE;
        do {
          *slot = gems;
          slot += 1;
          i += 1;
          gems += ACTOR_SIZE;
        } while (i < 5);
      }
      digits = g_abHudGemRollBlock;
      if (digits[0] == 4) {
        sprintf((char *)scratch, g_szFmtDSlashD, g_nHudLevelGemCachedCount,
                g_nHudLevelGemCachedCount);
        rec = g_pSpriteRecordWriteCursor;
        rect[0] = 0x5A;
        rect[1] = 0x24;
        rect[2] = 0xB40;
        BuildTextSpriteChain((char *)scratch, rect, 0x1C, TINT_HUD);
        rec--;
        phase = 0;
        while ((int)rec >= (int)g_pSpriteRecordWriteCursor) {
          rec->rotZ =
              ((unsigned short *)
                   g_anCosineLut)[(GEM_HOLD_TIMER * 4 + phase) & 0xFF] >>
              7;
          *slot = rec;
          slot += 1;
          rec--;
          phase += 0xC;
        }
      }
    }
    dragons = g_abHudDragonCounterBlock;
    if (dragons[0] != 0) {
      dragons += 0x1FB;
      i = 0;
    dragon_slot:
      *slot = dragons;
      slot += 1;
      i += 1;
      dragons += ACTOR_SIZE;
      if (i < 3) {
        goto dragon_slot;
      }
    }
    lives = g_abHudLivesCounterBlock;
    if (lives[0] != 0) {
      i = 0;
      lives += 0x302;
    life_slot:
      *slot = lives;
      slot += 1;
      i += 1;
      lives += ACTOR_SIZE;
      if (i < 3) {
        goto life_slot;
      }
    }
    eggs = g_abHudKeyCounterBlock;
    if (eggs[0] != 0 && g_nHudKeyCachedState == 1) {
      *slot = eggs + 0x408;
      slot += 1;
    }
    *slot = 0;
  }

  ring = g_abHudButterflyRingBlock;
  if (ring[0] != 0) {
    i = 0;
    if (g_nHudButterflyCachedCount > 0) {
      do {
        shade =
            ((short)((unsigned short *)
                         g_anCosineLut)[(g_nLightSweepAngle - ((i << 8) / 20)) &
                                        ANGLE8_MASK] >>
             7) +
            0x80;
        scratch[0] = shade;
        scratch[1] = shade;
        scratch[2] = shade;
        EmitTexturedQuad((short *)(g_abHudButterflyRingBlock + 0x4C2 + i * 8),
                         LEVEL_SPRITE_TEX_VIA_HUD, /* uv words */
                         (unsigned int *)scratch);
        i += 1;
      } while (i < BUTTERFLY_COUNT);
    }
  }

  ring = g_abHudEggIconBlock;
  if (ring[0] != 0) {
    i = 0;
    if (g_nHudEggCachedCount > 0) {
      do {
        quad = g_abHudEggIconBlock + 0x461 + i * 8;
        step = FRAME_CYCLE + i;
        uvFrame = (step % 9) * 8 + (g_abHudEggIconBlock + 0x569);
        EmitTexturedQuad((short *)quad, (unsigned int *)uvFrame, 0);
        i += 1;
      } while (i < EGG_COUNT);
    }
  }
}

#undef LEVEL_SPRITE_TEX_VIA_HUD
#undef GEM_HOLD_TIMER
#undef BUTTERFLY_COUNT
#undef EGG_COUNT
#undef FRAME_CYCLE
