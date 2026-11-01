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

/* g_nCameraCurrentMode / g_nCameraNextMode. Modes 0..6 and 0xF are the
 * per-Spyro-state picks (g_abSpyroStateCameraModeTable, by SpyroState); the
 * rest are entered by request bits, scripts and transitions. CAM_HELD marks a
 * mode SelectCameraMode keeps until its own exit condition instead of
 * re-reading the state table. */
enum CameraMode {
  CAM_MODE_STAND = 0,             /* idle, teeter, hurt */
  CAM_MODE_MOVE = 1,              /* walk/run/jog, jump, slide, lift */
  CAM_MODE_SIDESTEP = 2,          /* sidestep, knockback (Mode2Front/Reverse) */
  CAM_MODE_CHARGE = 3,            /* the charge states */
  CAM_MODE_FALL = 4,
  CAM_MODE_GLIDE = 5,             /* glide and flight (Mode5 bias pair) */
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

#endif /* OPEN_SPYRO_CAMERA_H */
