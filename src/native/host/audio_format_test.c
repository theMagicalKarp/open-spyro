/* src/native/host/audio_format_test.c — unit tests for audio_format.c (make
 * native-test). */

#include <string.h>

#include "audio_format.h"
#include "test.h"

/* --- frames per vblank --------------------------------------------------- */

TEST(audio_frames_per_vsync_alternate) {
  double carry = 0;
  int n736 = 0;

  for (int i = 0; i < 1000; i++) {
    int n = audio_frames_per_vsync(&carry);

    CHECK(n == 735 || n == 736);
    n736 += n == 736;
    CHECK(carry >= 0 && carry < 1);
  }
  /* 735.735... per vblank: about three in four are 736 */
  CHECK(n736 > 700 && n736 < 760);
}

TEST(audio_frames_per_vsync_keep_rate) {
  double carry = 0;
  long total = 0;

  /* 59.94 Hz for 100 s of game time is 5994 vblanks, 4,410,000 frames */
  for (int i = 0; i < 5994; i++) {
    total += audio_frames_per_vsync(&carry);
  }
  CHECK(total >= 4409999 && total <= 4410000);
}

/* --- WAV header ---------------------------------------------------------- */

static unsigned le32(const uint8_t *p) {
  return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24;
}
static unsigned le16(const uint8_t *p) { return p[0] | p[1] << 8; }

TEST(audio_wav_header_fields) {
  uint8_t h[AUDIO_WAV_HEADER];

  audio_wav_header(h, 4000);
  CHECK(memcmp(h, "RIFF", 4) == 0);
  CHECK_INT(le32(h + 4), 36 + 4000);
  CHECK(memcmp(h + 8, "WAVEfmt ", 8) == 0);
  CHECK_INT(le32(h + 16), 16);    /* fmt chunk size */
  CHECK_INT(le16(h + 20), 1);     /* PCM */
  CHECK_INT(le16(h + 22), 2);     /* channels */
  CHECK_INT(le32(h + 24), 44100); /* sample rate */
  CHECK_INT(le32(h + 28), 44100 * 4);
  CHECK_INT(le16(h + 32), 4);  /* block align */
  CHECK_INT(le16(h + 34), 16); /* bits per sample */
  CHECK(memcmp(h + 36, "data", 4) == 0);
  CHECK_INT(le32(h + 40), 4000);
}

TEST(audio_wav_header_empty_and_large) {
  uint8_t h[AUDIO_WAV_HEADER];

  audio_wav_header(h, 0);
  CHECK_INT(le32(h + 4), 36);
  CHECK_INT(le32(h + 40), 0);
  audio_wav_header(h, 0xFFFFFF00u);
  CHECK_INT(le32(h + 40), 0xFFFFFF00u);
  CHECK_INT(le32(h + 4), 0xFFFFFF24u);
}
