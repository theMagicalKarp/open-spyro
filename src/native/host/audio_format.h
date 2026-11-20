/* src/native/host/audio_format.h — the sound output's format (audio.c): the
 * sample frames per vblank and the --wav file header. Free of SDL so
 * native/tests can test it (audio_format_test.c). */

#ifndef OPEN_SPYRO_AUDIO_FORMAT_H
#define OPEN_SPYRO_AUDIO_FORMAT_H

#include <stdint.h>

enum {
  AUDIO_RATE = 44100, /* PSYZ_SPU_SAMPLE_RATE */
  AUDIO_CHANNELS = 2,
  AUDIO_FRAME_BYTES = AUDIO_CHANNELS * (int)sizeof(short),
  AUDIO_WAV_HEADER = 44,
};

/* Sample frames in one vblank: 44100 / 59.94 = 735.7, the fraction carried
   in *carry to the next. */
int audio_frames_per_vsync(double *carry);

/* The 44-byte RIFF/WAVE header for `data_bytes` of 16-bit stereo PCM. */
void audio_wav_header(uint8_t out[AUDIO_WAV_HEADER], uint32_t data_bytes);

#endif
