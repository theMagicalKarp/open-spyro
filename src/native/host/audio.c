/* src/native/host/audio.c — sound output.
 *
 * PsyZ's SDL backend lets an SDL audio thread pull samples from the SPU
 * emulation, but the SPU lives in the wasm module, which must only run on
 * one thread. So the host pushes instead: at every blocking VSync it tops an
 * SDL audio stream up to AUDIO_LEAD frames from the module's
 * native_audio_pull. The SPU then advances at the device's rate, as it does
 * under PsyZ's pull model.
 *
 * With --fast nothing reaches the device; the SPU advances by one vblank's
 * worth of samples per VSync, keeping it in step with game time.
 *
 * --wav also writes everything the SPU produced to a WAV file (headless
 * checks; with --fast it is the game's audio at game speed). */

#include <stdio.h>

#include <SDL3/SDL.h>

#include "native_host.h"

enum {
  AUDIO_RATE = 44100, /* PSYZ_SPU_SAMPLE_RATE */
  AUDIO_CHANNELS = 2,
  AUDIO_FRAME_BYTES = AUDIO_CHANNELS * (int)sizeof(short),
  AUDIO_LEAD = 2048, /* ~46 ms queued ahead of the device */
  AUDIO_PULL_MAX = 2048,
};

static SDL_AudioStream *stream;
static int audio_failed;
static double vsync_carry;
static FILE *wav;
static uint32_t wav_bytes;

static void wav_header(void) {
  uint32_t riff[11] = {
      0x46464952, 36 + wav_bytes, 0x45564157, /* "RIFF" size "WAVE" */
      0x20746D66, 16,                         /* "fmt " */
      1 | AUDIO_CHANNELS << 16,               /* PCM, channels */
      AUDIO_RATE, AUDIO_RATE * AUDIO_FRAME_BYTES,
      AUDIO_FRAME_BYTES | 16 << 16,           /* block align, bits */
      0x61746164, wav_bytes,                  /* "data" size */
  };

  fseek(wav, 0, SEEK_SET);
  fwrite(riff, sizeof(riff), 1, wav);
  fseek(wav, 0, SEEK_END);
}

int native_audio_wav(const char *path) {
  wav = fopen(path, "wb");
  if (wav == NULL) {
    return -1;
  }
  wav_header();
  return 0;
}

void native_audio_close(void) {
  if (wav != NULL) {
    wav_header();
    fclose(wav);
    wav = NULL;
  }
}

static void audio_open(void) {
  SDL_AudioSpec spec = {SDL_AUDIO_S16, AUDIO_CHANNELS, AUDIO_RATE};

  if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    fprintf(stderr, "native: no audio: %s\n", SDL_GetError());
    audio_failed = 1;
    return;
  }
  stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
                                     NULL, NULL);
  if (stream == NULL) {
    fprintf(stderr, "native: no audio: %s\n", SDL_GetError());
    audio_failed = 1;
    return;
  }
  SDL_ResumeAudioStreamDevice(stream);
}

/* Runs the SPU for `frames` sample frames; queues them unless discarding. */
static void audio_run(int frames, int discard) {
  while (frames > 0) {
    int n = frames < AUDIO_PULL_MAX ? frames : AUDIO_PULL_MAX;
    u32 buf = w2c_spyro_native_audio_pull(native_instance(), (u32)n);
    const void *pcm = psx_ptr(native_memory(), buf);

    if (!discard) {
      SDL_PutAudioStreamData(stream, pcm, n * AUDIO_FRAME_BYTES);
    }
    if (wav != NULL) {
      fwrite(pcm, AUDIO_FRAME_BYTES, (size_t)n, wav);
      wav_bytes += (uint32_t)(n * AUDIO_FRAME_BYTES);
    }
    frames -= n;
  }
}

/* Sample frames in one vblank: 44100 / 59.94 = 735.7, carried. */
static int native_frames_per_vsync(void) {
  double want = AUDIO_RATE / 59.94 + vsync_carry;
  int n = (int)want;

  vsync_carry = want - n;
  return n;
}

void native_audio_vsync(void) {
  if (native_fast()) {
    audio_run(native_frames_per_vsync(), 1);
    return;
  }
  if (stream == NULL && !audio_failed) {
    audio_open();
  }
  if (stream == NULL || SDL_AudioStreamDevicePaused(stream)) {
    /* No device to feed: still run the SPU at real time so voices finish
       (GetSpuVoiceKeyStatus) and streamed audio keeps advancing. */
    audio_run(native_frames_per_vsync(), 1);
    return;
  }
  int queued = SDL_GetAudioStreamQueued(stream) / AUDIO_FRAME_BYTES;
  if (queued < AUDIO_LEAD) {
    audio_run(AUDIO_LEAD - queued, 0);
  }
}

/* PsyZ's SDL layer pauses audio while the app is in the background. */
int Psyz_AudioIsPaused(void) {
  return stream == NULL || SDL_AudioStreamDevicePaused(stream);
}
void Psyz_AudioPause(void) {
  if (stream != NULL) {
    SDL_PauseAudioStreamDevice(stream);
  }
}
void Psyz_AudioUnpause(void) {
  if (stream != NULL) {
    SDL_ResumeAudioStreamDevice(stream);
  }
}
