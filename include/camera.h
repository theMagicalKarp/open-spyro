/* camera.h — the camera's six-word angle sets.
 *
 * The camera keeps three copies of the same six words: the target
 * (g_nCameraTargetYaw, 0x80076e30), the spring (g_nCameraSpringYaw,
 * 0x80076e48) and the draft (g_nCameraYaw, 0x80076e60). Mode changes re-sync
 * one set from another with a whole-struct copy. */

#ifndef OPEN_SPYRO_CAMERA_H
#define OPEN_SPYRO_CAMERA_H

typedef struct CameraAngles {
  int yaw;       /* 0x00 */
  int pitch;     /* 0x04 */
  int distance;  /* 0x08 */
  int roll;      /* 0x0C */
  int pitchBias; /* 0x10 */
  int yawBias;   /* 0x14 */
} CameraAngles;

/* Word indices of a camera parameter block (g_pCameraTargetParams, the
 * g_anCameraParam* presets, g_anCameraActiveTargetParams): the CameraAngles
 * order LoadCameraTargetAnglesFromMode loads them in. The yaw word is
 * world-space (12-bit); the rest are copied verbatim. */
enum CameraParamWord {
  CAMP_YAW = 0,
  CAMP_PITCH = 1,
  CAMP_DISTANCE = 2,
  CAMP_ROLL = 3,
  CAMP_PITCH_BIAS = 4,
  CAMP_YAW_BIAS = 5,
};

/* A scripted-camera vantage (0x38 bytes): where the camera sits and how it
 * is turned, plus the pose Spyro is re-seated in when the record is drawn.
 * g_aScriptedCamViews holds three ROM presets UpdateScriptedCameraView
 * picks between, then a fourth, the fall/death vantage, whose camPos the
 * callers overwrite with the live camera position before activating it
 * (they reach it as g_anSpyroDeathCamRecordBlock, its camPos). */
typedef struct ScriptedCamView {
  int spyroPos[3]; /* 0x00 Spyro's position when a preset is drawn */
  int spyroPitch;  /* 0x0C -> g_nSpyroBodyPitch */
  int spyroRoll;   /* 0x10 -> g_nSpyroBodyRoll */
  int spyroYaw;    /* 0x14 -> g_nSpyroBodyYaw */
  int camPos[3];   /* 0x18 */
  int camRoll;     /* 0x24 -> g_nCameraEulerRoll */
  int camPitch;    /* 0x28 -> g_nCameraEulerPitch */
  int camYaw;      /* 0x2C -> g_nCameraEulerYaw */
  int aimAtSpyro;  /* 0x30 1 = re-aim the camera at Spyro every frame */
  int range;       /* 0x34 retire once Spyro is farther than this */
} ScriptedCamView;

/* The record whose camPos is at `pos`. */
#define SCRIPTED_CAM_VIEW_AT_POS(pos) ((ScriptedCamView *)((int *)(pos) - 6))

/* Byte views onto the camera globals from the held bases the original keeps
 * live (g_anCameraEulerPitchBlock, g_anCameraViewMtx, g_apCameraAnchorBlock);
 * each is the named global's address. */
#define CAM_VIEW_MTX_VIA_EULER ((char *)g_anCameraEulerPitchBlock - 0x3A) /* g_anCameraViewMtx */
#define CAM_W2C_MTX_VIA_EULER ((char *)g_anCameraEulerPitchBlock - 0x4E)  /* g_anWorldToCameraRotMtx */
#define CAM_W2C_MTX_VIA_VIEW ((char *)g_anCameraViewMtx - 0x14)           /* g_anWorldToCameraRotMtx */
#define CAM_POS_VIA_ANCHOR ((char *)g_apCameraAnchorBlock - 0xA8)         /* g_anCameraPos */
#define CAM_YAW_VIA_ANCHOR ((char *)g_apCameraAnchorBlock - 0x40)         /* g_nCameraYaw */

/* g_nCameraCurrentMode / g_nCameraNextMode. Modes 0..6 and 0xF are the
 * per-Spyro-state picks (g_abSpyroStateCameraModeTable, by SpyroState); the
 * rest are entered by request bits, scripts and transitions. CAM_HELD marks a
 * mode SelectCameraMode keeps until its own exit condition instead of
 * re-reading the state table. */
enum CameraMode {
  CAM_MODE_STAND = 0,             /* idle, teeter, hurt */
  CAM_MODE_MOVE = 1,              /* walk/run/jog, jump, slide, lift */
  CAM_MODE_SIDESTEP = 2,          /* sidestep, knockback (FollowBehind/Facing, as MOVE) */
  CAM_MODE_CHARGE = 3,            /* the charge states */
  CAM_MODE_FALL = 4,
  CAM_MODE_GLIDE = 5,             /* glide and flight (GlideLeft/RightBias) */
  CAM_MODE_HAZARD = 6,
  CAM_MODE_SCRIPTED_TARGET = 7,   /* SPYRO_RQ_TARGET_CAMERA */
  CAM_MODE_FREE_LOOK = 9,         /* Triangle held while idle */
  CAM_MODE_REQUEST = 0xA,         /* SPYRO_RQ_CAMERA: request anchor + params */
  CAM_MODE_PORTAL_PATH = 0xB,        /* UpdatePortalPathCamera; SPYRO_RQ_PORTAL_PATH_CAMERA */
  CAM_MODE_SCRIPTED_FOLLOW = 0xC, /* SPYRO_RQ_FOLLOW_CAMERA */
  CAM_MODE_SCRIPTED_VANTAGE = 0xE, /* UpdateScriptedCameraView */
  CAM_MODE_BONK = 0xF,            /* SPYRO_ST_CHARGE_BONK */
  CAM_MODE_PASSIVE = 0x10,        /* passive-camera option, hold timer */
  CAM_MODE_PASSIVE_SETTLE = 0x11, /* passive re-engage after a mode change */
  CAM_MODE_LEVEL_TRANSITION = 0x12, /* UpdateLevelTransitionCamera */
};
#define CAM_HELD 0x80000000

/* g_dwActiveCameraOptions (saved in SaveGameBuffer.cameraOptions). The pause
 * menu's Camera option toggles the whole word between CAM_OPT_ACTIVE and
 * CAM_OPT_PASSIVE (new games start passive); only bits 0x10 / 0x40 are tested. */
#define CAM_OPT_ACTIVE 0x02          /* the "Active" setting (bit never tested) */
#define CAM_OPT_PASSIVE_FOLLOW 0x10  /* idle Spyro: drift into CAM_MODE_PASSIVE */
#define CAM_OPT_SMOOTH_SWING 0x40    /* clamp free-look/charge spring deltas to
                                      * +-0x2000; keep the yaw on sidestep/fall entry */
#define CAM_OPT_PASSIVE (CAM_OPT_ACTIVE | CAM_OPT_PASSIVE_FOLLOW | CAM_OPT_SMOOTH_SWING)

/* UpdateCameraSpringForMode tuning (12-bit angles). Spyro facing more than
 * FACING_ANGLE off the camera yaw switches MOVE / SIDESTEP / glide / fall to
 * the ...FollowFacing block; an R1 roll that far right / left of it
 * (GLIDE_BIAS_ANGLE) picks the glide bias blocks. The passive follow
 * re-engages REENGAGE_FRAMES after the camera leaves REENGAGE_YAW /
 * REENGAGE_PITCH (exclusive) of behind-Spyro; the first SETTLE_FRAMES of a
 * gamestate reload the target even while colliding; HAZARD stops springing
 * once the six deltas sum to SPRING_REST or less. Free look: the d-pad steps
 * the look integrator LOOK_DPAD_STEP a frame, the look is held within
 * +-LOOK_MAX_YAW and -LOOK_MAX_DOWN..LOOK_MAX_UP (looking down past
 * LOOK_PITCH_SPLIT moves the pitch bias instead), and Spyro's head follows
 * within +-LOOK_HEAD_YAW / +-LOOK_HEAD_PITCH. */
#define CAM_FACING_ANGLE 0x578
#define CAM_GLIDE_BIAS_ANGLE 0x100
#define CAM_REENGAGE_FRAMES 0x5A
#define CAM_REENGAGE_YAW 0x191
#define CAM_REENGAGE_PITCH 0x101
#define CAM_SETTLE_FRAMES 0x78
#define CAM_SPRING_REST 0x20
#define CAM_LOOK_DPAD_STEP 0x30
#define CAM_LOOK_MAX_YAW 0x400
#define CAM_LOOK_MAX_DOWN 0x3D0
#define CAM_LOOK_MAX_UP 0x500
#define CAM_LOOK_PITCH_SPLIT 0x140
#define CAM_LOOK_HEAD_YAW 0x300
#define CAM_LOOK_HEAD_PITCH 0x200

#endif /* OPEN_SPYRO_CAMERA_H */
