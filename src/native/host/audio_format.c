/* src/native/host/audio_format.c — see audio_format.h. */

#include "audio_format.h"

int audio_frames_per_vsync(double *carry) {
  double want = AUDIO_RATE / 59.94 + *carry;
  int n = (int)want;

  *carry = want - n;
  return n;
}

static void put32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
  p[3] = (uint8_t)(v >> 24);
}

void audio_wav_header(uint8_t out[AUDIO_WAV_HEADER], uint32_t data_bytes) {
  const uint32_t words[11] = {
      0x46464952, /* "RIFF" */
      36 + data_bytes,
      0x45564157, /* "WAVE" */
      0x20746D66, /* "fmt " */
      16,
      1 | AUDIO_CHANNELS << 16, /* PCM, channels */
      AUDIO_RATE,
      AUDIO_RATE * AUDIO_FRAME_BYTES,
      AUDIO_FRAME_BYTES | 16 << 16, /* block align, bits */
      0x61746164,                   /* "data" */
      data_bytes,
  };

  for (int i = 0; i < 11; i++) {
    put32(out + 4 * i, words[i]);
  }
}
