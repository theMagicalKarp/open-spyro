/* cd_music.h — the XA music stream (HandleMusicCommand, TickCdMusicStream).
 * Music plays as XA audio off the disc (CdlReadS with CdlModeRT); data reads
 * wait for the stream to report MUSIC_ST_IDLE. */

#ifndef OPEN_SPYRO_CD_MUSIC_H
#define OPEN_SPYRO_CD_MUSIC_H

/* HandleMusicCommand's cmd (also queued in g_nPendingMusicCommand). */
#define MUSIC_CMD_PLAY 1   /* start `track` from its first sector */
#define MUSIC_CMD_STOP 2   /* fade out, then pause */
#define MUSIC_CMD_STOP_4 4 /* same as MUSIC_CMD_STOP (StopAllSoundExceptMask's) */
#define MUSIC_CMD_RESUME 8 /* restart from g_dwCdMusicReadHead */

/* g_dwMusicStreamStatus: one state at a time. */
#define MUSIC_ST_PLAYING 0x10 /* CdlReadS issued; the stream tick follows the read head */
#define MUSIC_ST_IDLE 0x40    /* drive free: data reads may run */
#define MUSIC_ST_LOCKED 0x80  /* commands ignored (never set by the game code) */
#define MUSIC_ST_PAUSED 0x100 /* faded out and paused */
#define MUSIC_ST_FADING 0x200 /* fading out; g_nLastCdMusicCommand runs at volume 0 */

#endif /* OPEN_SPYRO_CD_MUSIC_H */
