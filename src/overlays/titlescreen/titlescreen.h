/* titlescreen.h — shared definitions for the titlescreen overlay C matches.
 * Extern names are our splat autolabels (PROVIDE()d by address at overlay
 * link), with structs anchored at the symbols the asm references. The save
 * image (SaveGame) is named from BuildSaveGameBuffer's field order and the
 * memory-card file header (McFileHeader) follows the standard PS1 save-block
 * header; TitleState fields are named from their use in Titlescreen_Update /
 * _Draw. Unread fields are padNN; sprite and state ids are in this header and title_state.h.
 */
#ifndef OVL_TITLESCREEN_H
#define OVL_TITLESCREEN_H

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;

#include "cd_music.h"
#include "pad.h"
#include "title_state.h"

/* ---- titlescreen sprite ids (Titlescreen_EmitSpriteQuad; a negative id
 * draws the sprite mirrored). The text sprites are named from the dialog
 * that shows them and the choice each option leads to. ---- */
enum {
  TITLE_SPR_LOGO = 0,
  TITLE_SPR_BORDER = 1,              /* left half; -1 = mirrored right half */
  TITLE_SPR_WORLD_NAME = 2,          /* + world index (save's levelId / 10 - 1) */
  TITLE_SPR_EMPTY_SLOT = 8,
  TITLE_SPR_SLOT_CURSOR = 9,
  TITLE_SPR_SLOT_MARK = 10,          /* under the cursor while confirming overwrite */
  TITLE_SPR_PRESS_START = 11,
  TITLE_SPR_DIGIT0 = 13,             /* + digit, the save's dragon count */
  TITLE_SPR_DRAGON_ICON = 23,
  TITLE_SPR_MSG_CHECKING = 24,       /* TITLE_CARD_PROBE / _READ */
  TITLE_SPR_MSG_NO_CARD = 25,        /* TITLE_CARD_NONE */
  TITLE_SPR_MSG_CARD_UNUSABLE = 26,  /* TITLE_CARD_UNUSABLE */
  TITLE_SPR_MSG_WARNING = 27,        /* first line of NO_SAVE_WARNING and FORMAT_CONFIRM */
  TITLE_SPR_MSG_NO_SAVE_L2 = 28,
  TITLE_SPR_MSG_NO_SAVE_L3 = 29,
  TITLE_SPR_MSG_NO_SAVE_L4 = 30,
  TITLE_SPR_MSG_CARD_IN_SLOT = 31,   /* + card port */
  TITLE_SPR_MSG_UNFORMATTED_L1 = 33,
  TITLE_SPR_MSG_UNFORMATTED_L2 = 34,
  TITLE_SPR_MSG_FORMAT_SURE_L1 = 35,
  TITLE_SPR_MSG_FORMAT_SURE_L2 = 36,
  TITLE_SPR_MSG_FORMATTING = 37,
  TITLE_SPR_MSG_FORMAT_DONE = 38,
  TITLE_SPR_MSG_FORMAT_FAILED = 39,
  TITLE_SPR_MSG_NO_FILE_L1 = 40,     /* TITLE_CARD_CREATE_ASK */
  TITLE_SPR_MSG_NO_FILE_L2 = 41,
  TITLE_SPR_MSG_CREATING = 42,
  TITLE_SPR_MSG_CARD_FULL_L1 = 43,
  TITLE_SPR_MSG_CARD_FULL_L2 = 44,
  TITLE_SPR_MSG_CREATE_FAILED = 45,
  TITLE_SPR_MSG_PICK_NEW_SLOT = 46,  /* TITLE_MENU_NEW_SLOT */
  TITLE_SPR_MSG_PICK_LOAD_SLOT = 47, /* TITLE_MENU_LOAD_SLOT */
  TITLE_SPR_CARD_SLOT_LABEL = 49,    /* + card port */
  TITLE_SPR_MSG_PICK_CARD = 51,      /* TITLE_CARD_PICK_SLOT */
  TITLE_SPR_OPT_NO_SAVE = 52,        /* continue without saving */
  TITLE_SPR_OPT_RETRY = 53,
  TITLE_SPR_OPT_CANCEL = 54,
  TITLE_SPR_OPT_FORMAT = 55,
  TITLE_SPR_OPT_CREATE = 56,
  TITLE_SPR_OPT_CONTINUE = 57,       /* single "continue" prompt */
  TITLE_SPR_OPT_LOAD_GAME = 58,
  TITLE_SPR_OPT_NEW_GAME = 59,
  TITLE_SPR_OPT_SLOT1 = 60,
  TITLE_SPR_OPT_SLOT2 = 61,
  TITLE_SPR_OPT_OVERWRITE = 62,
  TITLE_SPR_MSG_CARD_FULL_L3 = 63
};

/* Titlescreen_PlayUiSound ids (index into g_anTitleSampleOffsets), named
 * from where Titlescreen_Update plays them. */
enum {
  TITLE_SFX_MOVE = 0,        /* option / slot cursor moves */
  TITLE_SFX_SELECT = 1,      /* card picked, menu choice confirmed */
  TITLE_SFX_CARD_ACCESS = 2, /* card probe, format and create start */
  TITLE_SFX_PROMPT = 3,      /* a question dialog opens or is cancelled */
  TITLE_SFX_ERROR = 4,       /* no card, card full, failure, refused choice */
  TITLE_SFX_FADE_OUT = 5,    /* leaving the title (game or demo) */
  TITLE_SFX_LOGO_IN = 6,     /* logo lands */
  TITLE_SFX_START = 7,       /* Start pressed at the logo */
  TITLE_SFX_LOGO_OUT = 8,    /* logo slides away */
  TITLE_SFX_SUCCESS = 9      /* save picker reached, format done, crash demo */
};

/* sprite color/flash variants */
enum {
  TITLE_COLOR_NORMAL = 0,
  TITLE_COLOR_HIGHLIGHT = 1,         /* selected option, blinking */
  TITLE_COLOR_PROMPT = 2,            /* "press start" blink */
  TITLE_COLOR_DIMMED = 3             /* save slots outside the active picker */
};

/* ---- save file: the 0x600-byte image (save_game.h) ---- */
#include "cutscene_path.h"
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
} McFileHeader; /* 0x200 */

typedef struct {
  McFileHeader header;    /* 0x000 */
  SaveGameBuffer saves[3]; /* 0x200, 3 x 0x600 */
} TitleSaveFile;

/* ---- titlescreen state (0x5C bytes) ---- */
typedef struct {
  int mode;               /* 0x00 */
  int menuState;                /* 0x04 */
  int idleFrames;                 /* 0x08 */
  int animTimer;              /* 0x0C */
  int subState;             /* 0x10 */
  int selection;       /* 0x14 */
  int yesNoChoice;                 /* 0x18 load-confirm toggle: 0 yes, 1 no */
  int exitPath;                 /* 0x1C enum TitleExitPath (g_nTitleExitPath) */
  int savedSelection;/* 0x20 */
  int retryCount;        /* 0x24 */
  int cardPort;         /* 0x28 */
  int pad2C;           /* 0x2C */
  int cardStatus[2];        /* 0x30 */
  int needsFormat[2];   /* 0x38 per port: card reported McErrNotFormat */
  int mcCmd;    /* 0x40 */
  int mcResult;    /* 0x44 */
  int anySaveUsed;           /* 0x48 some save slot has levelVisited[0] */
  TitleSaveFile *file; /* 0x4C */
  SaveGameBuffer *apSaves[3];   /* 0x50 */
} TitleState;

extern TitleState g_nTitleMode;

/* ---- cutscene layout ---- */
extern CutscenePath *g_pPathTableHead; /* the intro loops through its frame */

/* ---- misc main-EXE state the overlay touches ---- */
extern u_char g_abTitleSlideInCurve[16];
extern u_char g_abTitleSlideOutCurve[16];
extern int g_nScreenFadeLevel;

/* frame double-buffer: draw-env @0 (r0/g0/b0 @+0x19), disp-env @0x5C */
typedef struct {
  u_char drawEnv[0x5C]; /* DRAWENV */
  u_char dispEnv[0x14]; /* DISPENV */
} FrameEnvPair;

extern FrameEnvPair *g_pActiveFrameDrawEnv;

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
#include "memcard.h"

/* ---- CD / music state ---- */
extern struct {
  int baseLba;         /* 0x00 g_nCdBaseLba */
  int readSectorCount;/* 0x04 g_nCdReadSectorCount */
  int cdlLoc;         /* 0x08 CdlLOC g_abCdlLocScratch */
  void *readDst;        /* 0x0C g_pCdReadDstBuffer */
  char cdlFile[24];      /* 0x10 CdlFILE */
  volatile int readInFlight; /* 0x28 g_nCdReadInFlight */
} g_nCdBaseLba;

extern struct {
  int track;      /* 0x000 g_nCurrentMusicTrack */
  int streamStatus;             /* 0x004 g_dwMusicStreamStatus */
  struct { int start, end; } trackLba[64]; /* 0x008 */
  int readHead;     /* 0x208 g_dwCdMusicReadHead */
  char pad20C[8];         /* 0x20C */
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
extern int g_bSceneSfxVoiceMarker;
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
extern int RestartTitleCardCheckOnTriangle(void);
extern void AdvanceCutsceneFrame(int);
extern void RotateLightVectorXZ(int);

/* overlay-local */
void Titlescreen_EmitSpriteQuad(int nX, int nY, int nSprite, int nColorId);
void Titlescreen_PlayUiSound(int nSoundId);

#endif
