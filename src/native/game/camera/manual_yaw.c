/* Native replacement for src/main/camera/camera.c's UpdateCameraManualYawInput,
 * with an optional right-stick camera turn (input.camera.*). The original game
 * never reads the right stick. */

#include "camera.h"
#include "globals.h"
#include "pad.h"
#include "pad_buttons.h"
#include "spyro_state.h"

/* Yaw drive L2/R2 write. The camera spring turns at a rate proportional to
 * it, so the stick's drive scales the turn speed. */
#define MANUAL_YAW_DRIVE 0x400
/* Deflection (of PAD_STICK_MAX) that counts as held at full, for the boost. */
#define BOOST_DEFLECTION 0x78

static int stick_on, invert;
static int speed_q8 = 0x100, boost_q8 = 0x100, boost_vsyncs;
static int boost_held; /* VSyncs held at full deflection, up to boost_vsyncs */

/* Set by the host from input.camera.* before the game starts: invert flips the
 * stick's direction, speed and boost
 * in Q8 (0x100 = the L2/R2 rate), the boost ramp in VSyncs. */
__attribute__((export_name("native_set_camera_stick"))) void
native_set_camera_stick(int on, int inv, int speed, int boost, int ramp) {
  stick_on = on;
  invert = inv;
  speed_q8 = speed;
  boost_q8 = boost;
  boost_vsyncs = ramp;
}

/* The right stick's turn drive: proportional to the X deflection (shaped by
 * the host, input.right_stick.*) times the speed; held at full deflection the
 * speed ramps up to boost times itself over boost_vsyncs. */
static int StickYawDrive(int x) {
  int rate = speed_q8;

  if (x >= BOOST_DEFLECTION || x <= -BOOST_DEFLECTION) {
    boost_held += g_nFrameStep;
    if (boost_held > boost_vsyncs) {
      boost_held = boost_vsyncs;
    }
    rate = boost_vsyncs == 0 ? rate * boost_q8 >> 8
                             : rate + (rate * (boost_q8 - 0x100) >> 8) *
                                          boost_held / boost_vsyncs;
  } else {
    boost_held = 0;
  }
  /* Stick right turns the way R2 does (L2 when inverted). */
  if (invert) {
    x = -x;
  }
  return -x * MANUAL_YAW_DRIVE / PAD_STICK_MAX * rate >> 8;
}

/* 0x80035f58: L2/R2 camera turn (the original, unchanged), then the right
 * stick when enabled. The buttons win over the stick. */
void UpdateCameraManualYawInput(void) {
  g_nCameraManualYawInput = 0;
  if (g_dwSpyroRequestMask & SPYRO_RQ_LOCK_ON) {
    boost_held = 0;
    return;
  }
  if (g_nCameraStuckFrames != 0) {
    boost_held = 0;
    return;
  }
  if (g_dwPadHeld & PAD_R2) {
    g_nCameraManualYawInput = -MANUAL_YAW_DRIVE;
  } else if (g_dwPadHeld & PAD_L2) {
    g_nCameraManualYawInput = MANUAL_YAW_DRIVE;
  } else if (stick_on && g_nPadType == PAD_TYPE_ANALOG) {
    int x = ((PadAnalog *)&g_dwPadAnalogCalibrated)->axis[PAD_AXIS_RX] -
            PAD_STICK_CENTER;

    if (x != 0) {
      g_nCameraManualYawInput = StickYawDrive(x);
      /* A held L2/R2 clears the pad's all-input-idle flag, which restarts the
         standing camera's re-engage countdown (UpdateCameraSpringForMode);
         the right stick is input too. */
      if (g_nCameraCurrentMode == CAM_MODE_STAND ||
          g_nCameraCurrentMode == (int)(CAM_HELD | CAM_MODE_STAND)) {
        g_nCameraSpringReengageCountdown = CAM_REENGAGE_FRAMES;
      }
      return;
    }
  }
  boost_held = 0;
}
