/* src/native/game/camera/manual_yaw_test.c — unit tests for manual_yaw.c
 * (make native-test).
 *
 * manual_yaw.c is included, not linked, so the tests can reset its boost
 * ramp; the game globals it reads and writes are defined here. The game
 * headers' wasm32 typedefs that the host's own headers define differently are
 * renamed around the include. */

#define __ssize_t game___ssize_t
#define ssize_t game_ssize_t
#define __jmp_buf game___jmp_buf
#include "manual_yaw.c"
#undef __ssize_t
#undef ssize_t
#undef __jmp_buf

#include "test.h"

unsigned int g_dwSpyroRequestMask;
int g_nCameraStuckFrames;
unsigned int g_dwPadHeld;
int g_nPadType;
unsigned int g_dwPadAnalogCalibrated;
int g_nFrameStep;
int g_nCameraCurrentMode;
int g_nCameraSpringReengageCountdown;
int g_nCameraManualYawInput;

/* An analog pad at rest, one VSync per frame, the stick turn on at the L2/R2
   rate with no boost. */
static void reset(void) {
  g_dwSpyroRequestMask = 0;
  g_nCameraStuckFrames = 0;
  g_dwPadHeld = 0;
  g_nPadType = PAD_TYPE_ANALOG;
  g_dwPadAnalogCalibrated = 0x7F7F7F7F;
  g_nFrameStep = 1;
  g_nCameraCurrentMode = 1; /* not CAM_MODE_STAND */
  g_nCameraSpringReengageCountdown = 0;
  g_nCameraManualYawInput = 0x1234;
  native_set_camera_stick(1, 0, 0x100, 0x100, 0);
  boost_held = 0;
}

/* Sets the right stick's calibrated X byte (PAD_STICK_CENTER = centred). */
static void right_x(int v) {
  ((PadAnalog *)&g_dwPadAnalogCalibrated)->axis[PAD_AXIS_RX] = (unsigned char)v;
}

/* One frame with the right stick at `x` from centre; returns the drive. */
static int turn(int x) {
  right_x(PAD_STICK_CENTER + x);
  UpdateCameraManualYawInput();
  return g_nCameraManualYawInput;
}

TEST(manual_yaw_buttons_as_original) {
  reset();
  native_set_camera_stick(0, 0, 0x100, 0x100, 0);
  UpdateCameraManualYawInput();
  CHECK_INT(g_nCameraManualYawInput, 0);
  g_dwPadHeld = PAD_R2;
  UpdateCameraManualYawInput();
  CHECK_INT(g_nCameraManualYawInput, -0x400);
  g_dwPadHeld = PAD_L2;
  UpdateCameraManualYawInput();
  CHECK_INT(g_nCameraManualYawInput, 0x400);
  g_dwPadHeld = PAD_L2 | PAD_R2; /* R2 is tested first */
  UpdateCameraManualYawInput();
  CHECK_INT(g_nCameraManualYawInput, -0x400);
}

TEST(manual_yaw_suppressed) {
  reset();
  g_dwPadHeld = PAD_R2;
  g_dwSpyroRequestMask = SPYRO_RQ_LOCK_ON;
  CHECK_INT(turn(0x7F), 0);
  g_dwSpyroRequestMask = 0;
  g_nCameraStuckFrames = 3;
  CHECK_INT(turn(0x7F), 0);
}

TEST(manual_yaw_buttons_beat_stick) {
  reset();
  g_dwPadHeld = PAD_L2;
  CHECK_INT(turn(0x7F), 0x400);
}

TEST(manual_yaw_stick_needs_setting_and_analog_pad) {
  reset();
  native_set_camera_stick(0, 0, 0x100, 0x100, 0);
  CHECK_INT(turn(0x7F), 0);
  reset();
  g_nPadType = PAD_TYPE_ANALOG - 1; /* digital */
  CHECK_INT(turn(0x7F), 0);
  reset();
  CHECK_INT(turn(0), 0);
}

/* Stick right turns as R2 does, proportional to deflection. */
TEST(manual_yaw_stick_proportional) {
  reset();
  CHECK_INT(turn(0x7F), -0x400);
  CHECK_INT(turn(-0x7F), 0x400);
  CHECK_INT(turn(64), -64 * 0x400 / 0x7F);
  CHECK_INT(turn(-64), 64 * 0x400 / 0x7F);
}

TEST(manual_yaw_stick_invert) {
  reset();
  native_set_camera_stick(1, 1, 0x100, 0x100, 0);
  CHECK_INT(turn(0x7F), 0x400);
  CHECK_INT(turn(-64), -64 * 0x400 / 0x7F);
}

TEST(manual_yaw_stick_speed) {
  reset();
  native_set_camera_stick(1, 0, 0x200, 0x100, 0);
  CHECK_INT(turn(64), -(64 * 0x400 / 0x7F) * 2);
  CHECK_INT(turn(0x7F), -0x800);
}

/* Held at full deflection the rate ramps from 1x to boost over the ramp's
   VSyncs, then holds. */
TEST(manual_yaw_boost_ramps_and_caps) {
  reset();
  native_set_camera_stick(1, 0, 0x100, 0x200, 10);
  CHECK_INT(turn(0x7F), -0x400 * (0x100 + 0x100 / 10) >> 8);
  for (int i = 2; i < 10; i++) {
    turn(0x7F);
  }
  CHECK_INT(turn(0x7F), -0x800);
  CHECK_INT(turn(0x7F), -0x800);
  CHECK_INT(boost_held, 10);
}

/* The ramp counts VSyncs, so a 2-VSync frame advances it twice as far. */
TEST(manual_yaw_boost_counts_vsyncs) {
  reset();
  native_set_camera_stick(1, 0, 0x100, 0x200, 10);
  g_nFrameStep = 2;
  turn(0x7F);
  CHECK_INT(boost_held, 2);
}

TEST(manual_yaw_boost_without_ramp_is_immediate) {
  reset();
  native_set_camera_stick(1, 0, 0x100, 0x300, 0);
  CHECK_INT(turn(0x7F), -0xC00);
}

/* Below BOOST_DEFLECTION there is no boost. */
TEST(manual_yaw_boost_needs_full_deflection) {
  reset();
  native_set_camera_stick(1, 0, 0x100, 0x200, 0);
  CHECK_INT(turn(BOOST_DEFLECTION - 1), -(BOOST_DEFLECTION - 1) * 0x400 / 0x7F);
  CHECK_INT(boost_held, 0);
  CHECK_INT(turn(BOOST_DEFLECTION), -BOOST_DEFLECTION * 0x400 / 0x7F * 2);
}

/* Anything that stops the full-deflection hold restarts the ramp. */
TEST(manual_yaw_boost_resets) {
  static const char *const why[] = {"eased off", "released", "lock-on", "stuck",
                                    "button"};

  for (int i = 0; i < 5; i++) {
    reset();
    native_set_camera_stick(1, 0, 0x100, 0x200, 10);
    for (int f = 0; f < 5; f++) {
      turn(0x7F);
    }
    CHECK_INT(boost_held, 5);
    switch (i) {
    case 0:
      turn(BOOST_DEFLECTION - 1);
      break;
    case 1:
      turn(0);
      break;
    case 2:
      g_dwSpyroRequestMask = SPYRO_RQ_LOCK_ON;
      turn(0x7F);
      g_dwSpyroRequestMask = 0;
      break;
    case 3:
      g_nCameraStuckFrames = 1;
      turn(0x7F);
      g_nCameraStuckFrames = 0;
      break;
    case 4:
      g_dwPadHeld = PAD_R2;
      turn(0x7F);
      g_dwPadHeld = 0;
      break;
    }
    if (boost_held != 0) {
      TEST_FAIL_("boost_held == %d after %s, want 0", boost_held, why[i]);
    }
    CHECK_INT(turn(0x7F), -0x400 * (0x100 + 0x100 / 10) >> 8);
  }
}

/* The stick, like L2/R2, counts as input to the standing camera. */
TEST(manual_yaw_stick_restarts_reengage_countdown) {
  reset();
  turn(0x7F);
  CHECK_INT(g_nCameraSpringReengageCountdown, 0);
  g_nCameraCurrentMode = CAM_MODE_STAND;
  turn(0x7F);
  CHECK_INT(g_nCameraSpringReengageCountdown, CAM_REENGAGE_FRAMES);
  g_nCameraSpringReengageCountdown = 0;
  g_nCameraCurrentMode = (int)(CAM_HELD | CAM_MODE_STAND);
  turn(0x7F);
  CHECK_INT(g_nCameraSpringReengageCountdown, CAM_REENGAGE_FRAMES);
  g_nCameraSpringReengageCountdown = 0;
  turn(0);
  CHECK_INT(g_nCameraSpringReengageCountdown, 0);
}
