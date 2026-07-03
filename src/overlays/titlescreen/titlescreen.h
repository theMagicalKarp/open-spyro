/* titlescreen.h — shared definitions for the titlescreen overlay C matches.
 * Extern names are our splat autolabels (PROVIDE()d by address at overlay
 * link), with structs anchored at the symbols the asm references. Field/enum
 * names are offset- or value-based (unk_0x.., Sprite_0x..) since only the
 * layout, not any semantic name, is asserted by the byte-match.
 */
#ifndef OVL_TITLESCREEN_H
#define OVL_TITLESCREEN_H

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;

/* ---- titlescreen sprite / color ids ---- */
enum {
  Sprite_0x00 = 0,
  Sprite_0x01 = 1,
  Sprite_0x02 = 2,
  Sprite_0x08 = 8,
  Sprite_0x09 = 9,
  Sprite_0x0A = 10,
  Sprite_0x0B = 11,
  Sprite_0x0D = 13,
  Sprite_0x17 = 23,
  Sprite_0x18 = 24,
  Sprite_0x19 = 25,
  Sprite_0x1A = 26,
  Sprite_0x1B = 27,
  Sprite_0x1C = 28,
  Sprite_0x1D = 29,
  Sprite_0x1E = 30,
  Sprite_0x1F = 31,
  Sprite_0x21 = 33,
  Sprite_0x22 = 34,
  Sprite_0x23 = 35,
  Sprite_0x24 = 36,
  Sprite_0x25 = 37,
  Sprite_0x26 = 38,
  Sprite_0x27 = 39,
  Sprite_0x28 = 40,
  Sprite_0x29 = 41,
  Sprite_0x2A = 42,
  Sprite_0x2B = 43,
  Sprite_0x2C = 44,
  Sprite_0x2D = 45,
  Sprite_0x2E = 46,
  Sprite_0x2F = 47,
  Sprite_0x31 = 49,
  Sprite_0x33 = 51,
  Sprite_0x34 = 52,
  Sprite_0x35 = 53,
  Sprite_0x36 = 54,
  Sprite_0x37 = 55,
  Sprite_0x38 = 56,
  Sprite_0x39 = 57,
  Sprite_0x3A = 58,
  Sprite_0x3B = 59,
  Sprite_0x3C = 60,
  Sprite_0x3D = 61,
  Sprite_0x3E = 62,
  Sprite_0x3F = 63
};

enum {
  Color_0x00 = 0,
  Color_0x01 = 1,
  Color_0x02 = 2,
  Color_0x03 = 3
};

/* menu substates */
enum {
  SubState_0x00 = 0,
  SubState_0x01 = 1,
  SubState_0x02 = 2,
  SubState_0x03 = 3,
  SubState_0x04 = 4,
  SubState_0x05 = 5,
  SubState_0x06 = 6,
  SubState_0x07 = 7,
  SubState_0x08 = 8,
  SubState_0x09 = 9,
  SubState_0x0A = 10,
  SubState_0x0B = 11,
  SubState_0x0C = 12,
  SubState_0x0D = 13,
  SubState_0x0F = 15
};

/* top-level titlescreen modes */
enum { Mode_0x00 = 0, Mode_0x01 = 1, Mode_0x02 = 2, Mode_0x03 = 3 };

/* ---- save file (offsets are what matter) ---- */
#define TOTAL_LEVEL_COUNT 36

typedef struct {
  int unk_0x00[5];         /* 0x00 */
  u_char unk_0x14[5][5];   /* 0x14 */
} Struct_0x30;              /* 0x30 */

typedef struct {
  u_char unk_0x00;                     /* 0x00 */
  u_char unk_0x01;                     /* 0x01 */
  u_char unk_0x02;                     /* 0x02 */
  u_char unk_0x03;                     /* 0x03 */
  int unk_0x04;                        /* 0x04 */
  u_char unk_0x08;                     /* 0x08 */
  u_char unk_0x09;                     /* 0x09 */
  u_char unk_0x0A;                     /* 0x0A */
  u_char unk_0x0B;                     /* 0x0B */
  int unk_0x0C;                        /* 0x0C */
  Struct_0x30 unk_0x10;                /* 0x10 */
  u_char unk_0x40[TOTAL_LEVEL_COUNT];  /* 0x40 */
  u_char unk_0x64[TOTAL_LEVEL_COUNT];  /* 0x64 */
  u_char unk_0x88[TOTAL_LEVEL_COUNT];  /* 0x88 */
  short unk_0xAC[TOTAL_LEVEL_COUNT];   /* 0xAC */
  u_char unk_0xF4[18];                 /* 0xF4 */
  u_char unk_0x106[6];                 /* 0x106 */
  u_char unk_0x10C[TOTAL_LEVEL_COUNT * 32]; /* 0x10C */
  int unk_0x58C;                       /* 0x58C */
  char pad[0x70];
} Struct_0x600;

typedef struct {
  char unk_0x00[2];
  u_char unk_0x02;
  u_char unk_0x03;
  u_char unk_0x04[0x40];
  char reserved[0x1C];
  u_short unk_0x60[0x10];
  u_char unk_0x80[0x80][3];
} Struct_0x200; /* 0x200 */

typedef struct {
  Struct_0x200 unk_0x000;    /* 0x000 */
  Struct_0x600 unk_0x200[3]; /* 0x200, 3 x 0x600 */
} Struct_0x1A00;

/* ---- titlescreen state (0x5C bytes) ---- */
typedef struct {
  int unk_0x00;               /* 0x00 */
  int unk_0x04;                /* 0x04 */
  int unk_0x08;                 /* 0x08 */
  int unk_0x0C;              /* 0x0C */
  int unk_0x10;             /* 0x10 */
  int unk_0x14;       /* 0x14 */
  int unk_0x18;                 /* 0x18 */
  int unk_0x1C;                 /* 0x1C */
  int unk_0x20;/* 0x20 */
  int unk_0x24;        /* 0x24 */
  int unk_0x28;         /* 0x28 */
  int unk_0x2C;        /* 0x2C */
  int unk_0x30[2];        /* 0x30 */
  int unk_0x38[2];   /* 0x38 */
  int unk_0x40;    /* 0x40 */
  int unk_0x44;    /* 0x44 */
  int unk_0x48;           /* 0x48 */
  Struct_0x1A00 *unk_0x4C; /* 0x4C */
  Struct_0x600 *unk_0x50[3];   /* 0x50 */
} Struct_0x5C;

extern Struct_0x5C D_80078D78;

/* ---- cutscene layout ---- */
typedef struct {
  int unk_0x00; /* 0x00 */
  int unk_0x04;
  int unk_0x08;
  int unk_0x0C;
} Struct_0x10;

extern Struct_0x10 *D_80075680;

/* ---- misc main-EXE state the overlay touches ---- */
extern u_char D_8006FA74[16];
extern u_char D_8006FA84[16];
extern int D_80075918;

/* frame double-buffer: draw-env @0 (r0/g0/b0 @+0x19), disp-env @0x5C */
typedef struct {
  u_char unk_0x00[0x5C];
  u_char unk_0x5C[0x14];
} Struct_0x70;

extern Struct_0x70 *D_80075888;
/* background-color bytes of the two frame buffers' draw-env (r0,g0,b0) */
typedef struct {
  u_char r, g, b;
} Struct_0x03;
extern Struct_0x03 D_80076EF9;
extern Struct_0x03 D_80076F7D;
extern Struct_0x03 D_80078A50;

extern struct {
  int post; /* 0x80075950 */
  int pre;  /* 0x80075954 */
} D_80075950; /* vsync frame pacing pair */

extern u_char D_8006FCF4[];  /* scratch image buffer */
extern int D_800785D0;       /* far-plane culling distance */
extern char D_80076DE4[];    /* camera view matrix (proj matrix at -0x14) */

/* ---- main-EXE functions this overlay calls ---- */
extern void func_80016914(void *, int, int);
extern void func_800521C0(void);
extern void func_8001F158(void);
extern void func_8001F798(void);
extern void func_800258F0(int);
extern void func_8004EBA8(int, void *, void *);
extern void func_800190D4(int, int, int, int);
extern void func_8005F764(int);
extern int func_8005DBC4(int);
extern void func_80060030(void *);
extern void func_8005FDD8(void *);
extern void *func_80016784(int);
extern void func_8005FD64(void *);

/* ---- gamepad ---- */
#define PAD_L1 (1 << 2)
#define PAD_TRIANGLE (1 << 4)
#define PAD_CROSS (1 << 6)
#define PAD_START (1 << 11)
#define PAD_UP (1 << 12)
#define PAD_RIGHT (1 << 13)
#define PAD_DOWN (1 << 14)
#define PAD_LEFT (1 << 15)

extern struct {
  int unk_0x0; /* buttons pressed this frame */
  int unk_0x4;
  int unk_0x8;
  int unk_0xC;
} D_80077378;

/* ---- memory card error codes (PSY-Q libmcrd) ---- */
#define McErrNone 0
#define McErrCardNotExist 1
#define McErrNewCard 3
#define McErrNotFormat 4
#define McErrFileNotExist 5
#define McErrBlockFull 7

/* ---- CD / music state ---- */
extern struct {
  int unk_0x00;         /* 0x00 */
  int unk_0x04;         /* 0x04 */
  int unk_0x08;         /* 0x08 CdlLOC */
  void *unk_0x0C;        /* 0x0C */
  char unk_0x10[24];     /* 0x10 CdlFILE */
  volatile int unk_0x28;/* 0x28 */
} D_80076B90;

extern struct {
  int unk_0x000;      /* 0x000 */
  int unk_0x004;             /* 0x004 */
  struct { int start, end; } unk_0x008[64]; /* 0x008 */
  int unk_0x208;     /* 0x208 */
  char unk_0x20C[8];      /* 0x20C */
  int unk_0x214;   /* 0x214 */
  int unk_0x218;            /* 0x218 */
  int unk_0x21C;    /* 0x21C */
} D_800774B0;

/* ---- fairy cutscene tail (this struct's field names are absolute byte
   offsets within the larger unmatched structure) ---- */
extern struct {
  int unk_0x14;
  int unk_0x18;
  int unk_0x1c;
  int unk_0x20;
} D_80078D14;

/* camera rotation svec */
extern struct {
  short x, y, z;
} D_80076E1C;

typedef struct {
  short x, y, w, h;
} RECT;
#define setRECT(r, _x, _y, _w, _h) \
  (r)->x = (_x), (r)->y = (_y), (r)->w = (_w), (r)->h = (_h)

extern int D_8007579C;
extern int D_800758E8;
extern int D_8007580C;
extern int D_80078BBC;
extern int D_80077084;
extern u_char D_8006F494[512];
extern char *D_800785E8;
extern int D_8007A6D8;

extern int func_8006272C(void);
extern void func_80056200(int, int);
extern void func_800163E4(void);
extern int func_80063BD8(int, int);
extern void func_80016698(int, void *, int, int, int);
extern void func_800567F4(int, int);
extern void func_8005FA28(RECT *, void *);
extern void func_8005F8F8(RECT *, int, int, int);
extern void func_80056B28(int);
extern void func_80056ED4(void);
extern void func_8005637C(void);
extern void func_8002BBE0(void);
extern void func_8002BAB8(void);
extern void func_800662BC(void);
extern int func_80067628(int, long *, long *);
extern void func_800665B8(int);
extern void func_80066E28(int, char *, unsigned long *, int, int);
extern void func_800670E4(int, char *, unsigned long *, int, int);
extern int func_80067718(int, char *, int);
extern int func_80067B28(int);
extern void func_8006631C(void);
extern int func_8005956C(u_char *);
extern void func_80059864(Struct_0x600 *);
extern int func_80059594(Struct_0x600 *);
extern void func_80032A20(void);
extern void func_80016958(void *, void *, int);
extern int func_80017FD4(void);
extern void func_8001277C(void);
extern int func_80032AB0(void);
extern void func_8002BFE0(int);
extern void func_80058CC0(int);

/* overlay-local */
void func_titlescreen_8007CD38(int nX, int nY, int nSprite, int nColorId);
void func_titlescreen_8007AAD4(int nSoundId);

#endif
