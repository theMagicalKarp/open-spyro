/* titlescreen.h — shared definitions for the titlescreen overlay C matches.
 * Extern names are our splat autolabels (PROVIDE()d by address at overlay
 * link), with structs anchored at the symbols the asm references. The save
 * image (SaveGame) is named from BuildSaveGameBuffer's field order and the
 * memory-card file header (McFileHeader) follows the standard PS1 save-block
 * header; TitleState fields are named from their use in Titlescreen_Update /
 * _Draw. Remaining unk_0x.. fields and the Sprite_0x.. ids keep their offset /
 * value names.
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

/* ---- save file: the 0x600-byte image (save_game.h) ---- */
#include "fog_color.h"
#include "save_game.h"

typedef struct {
  char magic[2];
  u_char iconFlag;
  u_char blockCount;
  u_char title[0x40];
  char reserved[0x1C];
  u_short clut[0x10];
  u_char icon[0x80][3];
} Struct_0x200; /* 0x200 */

typedef struct {
  Struct_0x200 header;    /* 0x000 */
  SaveGameBuffer saves[3]; /* 0x200, 3 x 0x600 */
} Struct_0x1A00;

/* ---- titlescreen state (0x5C bytes) ---- */
typedef struct {
  int mode;               /* 0x00 */
  int menuState;                /* 0x04 */
  int idleFrames;                 /* 0x08 */
  int animTimer;              /* 0x0C */
  int subState;             /* 0x10 */
  int selection;       /* 0x14 */
  int yesNoChoice;                 /* 0x18 load-confirm toggle: 0 yes, 1 no */
  int demoType;                 /* 0x1C */
  int savedSelection;/* 0x20 */
  int retryCount;        /* 0x24 */
  int cardPort;         /* 0x28 */
  int unk_0x2C;        /* 0x2C */
  int cardStatus[2];        /* 0x30 */
  int needsFormat[2];   /* 0x38 per port: card reported McErrNotFormat */
  int mcCmd;    /* 0x40 */
  int mcResult;    /* 0x44 */
  int anySaveUsed;           /* 0x48 some save slot has levelVisited[0] */
  Struct_0x1A00 *file; /* 0x4C */
  SaveGameBuffer *apSaves[3];   /* 0x50 */
} Struct_0x5C;

extern Struct_0x5C g_nTitleMode;

/* ---- cutscene layout ---- */
typedef struct {
  int introFrame; /* 0x00 */
  int unk_0x04;
  int unk_0x08;
  int unk_0x0C;
} Struct_0x10;

extern Struct_0x10 *g_pPathTableHead;

/* ---- misc main-EXE state the overlay touches ---- */
extern u_char g_abTitleSlideInCurve[16];
extern u_char g_abTitleSlideOutCurve[16];
extern int g_nGenericCountdown;

/* frame double-buffer: draw-env @0 (r0/g0/b0 @+0x19), disp-env @0x5C */
typedef struct {
  u_char drawEnv[0x5C]; /* DRAWENV */
  u_char dispEnv[0x14]; /* DISPENV */
} Struct_0x70;

extern Struct_0x70 *g_pActiveFrameDrawEnv;

extern struct {
  int post; /* 0x80075950 */
  int pre;  /* 0x80075954 */
} g_nVsyncFramePaceAnchor; /* vsync frame pacing pair */

extern u_char g_pOtDepthBinHead0[];  /* scratch image buffer */
extern int g_nFarPlaneDrawDistance;       /* far-plane culling distance */
extern char g_anCameraViewMtx[];    /* camera view matrix (proj matrix at -0x14) */

/* ---- main-EXE functions this overlay calls ---- */
extern void FillWord(void *, int, int);
extern void BuildRenderEntityLists(void);
extern void BuildActorDrawList(void);
extern void EmitActorDrawList(void);
extern void RenderWorldChunks(int);
extern void EmitStaticActorMeshList(int, void *, void *);
extern void DrawFullscreenTint(int, int, int, int);
extern void DrawSync(int);
extern int VSync(int);
extern void PutDispEnv(void *);
extern void PutDrawEnv(void *);
extern void *LinkOTPrimitives(int);
extern void DrawOTag(void *);

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
  int pressed;  /* buttons pressed this frame */
  int released; /* g_dwPadReleased */
  int held;     /* g_dwPadHeld */
  int padType;  /* g_nPadType */
} g_dwPadPressed;

/* ---- memory card error codes (PSY-Q libmcrd) ---- */
#define McErrNone 0
#define McErrCardNotExist 1
#define McErrNewCard 3
#define McErrNotFormat 4
#define McErrFileNotExist 5
#define McErrBlockFull 7

/* ---- CD / music state ---- */
extern struct {
  int baseLba;         /* 0x00 g_nCdBaseLba */
  int readSectorCount;/* 0x04 g_nCdReadSectorCount */
  int cdlLoc;         /* 0x08 CdlLOC g_abCdlLocScratch */
  void *readDst;        /* 0x0C g_pCdReadDstBuffer */
  char unk_0x10[24];     /* 0x10 CdlFILE */
  volatile int readInFlight; /* 0x28 g_nCdReadInFlight */
} g_nCdBaseLba;

extern struct {
  int track;      /* 0x000 g_nCurrentMusicTrack */
  int streamStatus;             /* 0x004 g_dwMusicStreamStatus */
  struct { int start, end; } trackLba[64]; /* 0x008 */
  int readHead;     /* 0x208 g_dwCdMusicReadHead */
  char unk_0x20C[8];      /* 0x20C */
  int lastCommand;   /* 0x214 g_nLastCdMusicCommand */
  int pendingCommand;            /* 0x218 g_nPendingMusicCommand */
  int streamTimeout;    /* 0x21C g_nCdMusicStreamTimeout */
} g_nCurrentMusicTrack;

/* The memory-card save slot the game was loaded from (main 0x80078D14..0x20,
 * read back by the save menu, SaveMenu_Update), viewed as one record. */
typedef struct SaveSlotInfo {
  int loaded;   /* 0x14 g_nSaveSlotLoaded: a slot is known, saving is offered */
  int cardPort; /* 0x18 memory-card port << 4 */
  int slot;     /* 0x1C g_nSaveSlotIndex: slot within the save file */
  int fileTag;  /* 0x20 g_nSaveFileTag: id stamped into the file header's
                 * clut[15]; the save menu checks it before writing */
} SaveSlotInfo;
extern SaveSlotInfo g_nSaveSlotLoaded_s;

/* camera rotation svec */
extern struct {
  short x, y, z;
} g_nCameraEulerRoll;

typedef struct {
  short x, y, w, h;
} RECT;
#define setRECT(r, _x, _y, _w, _h) \
  (r)->x = (_x), (r)->y = (_y), (r)->w = (_w), (r)->h = (_h)

extern int g_nGamePaused;
extern int g_nSparxFoodCount;
extern int g_nSparxHealthStash;
extern int g_nSparxHealth;
extern int g_bDragonRescueSfxVoiceMarker;
extern u_char g_abSaveFileHeaderTemplate[512];
extern char *g_pRenderScratchRegionBase;
extern int g_nTitleSceneSectorOffset;

extern int GetRandomU32(void);
extern void StopSoundVoiceByIndex(int, int);
extern void RestartStalledCdRead(void);
extern int CallCdSync(int, int);
extern void StartCdReadAsync(int, void *, int, int, int);
extern void HandleMusicCommand(int, int);
extern void LoadImage(RECT *, void *);
extern void ClearImage(RECT *, int, int, int);
extern void StopAllSoundExceptMask(int);
extern void TickSpuPerFrame(void);
extern void TickActiveSoundVoices(void);
extern void TickCdMusicStream(void);
extern void LaunchCrashDemo(void);
extern void MemCardOpenSession(void);
extern int MemCardSync(int, long *, long *);
extern void MemCardLoad(int);
extern void MemCardRead(int, char *, unsigned long *, int, int);
extern void MemCardWrite(int, char *, unsigned long *, int, int);
extern int MemCardCreateFile(int, char *, int);
extern int MemCardFormat(int);
extern void MemCardCloseSession(void);
extern int ComputeSaveGameChecksum(u_char *);
extern void BuildSaveGameBuffer(SaveGameBuffer *);
extern int ApplySaveGameBuffer(SaveGameBuffer *);
extern void PollMemCardSyncRing(void);
extern void CopyWords(void *, void *, int);
extern int ReadRootCounter2(void);
extern void InitNewGameState(void);
extern int ArmStoryIntroOnStart(void);
extern void AdvanceGemCutsceneFrame(int);
extern void RotateLightVectorXZ(int);

/* overlay-local */
void Titlescreen_EmitSpriteQuad(int nX, int nY, int nSprite, int nColorId);
void Titlescreen_PlayUiSound(int nSoundId);

#endif
