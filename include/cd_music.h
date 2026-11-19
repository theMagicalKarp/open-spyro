/* cd_music.h — the XA music stream (HandleMusicCommand, TickCdMusicStream).
 * Music plays as XA audio off the disc (CdlReadS with CdlModeRT); data reads
 * wait for the stream to report MUSIC_ST_IDLE. */

#ifndef OPEN_SPYRO_CD_MUSIC_H
#define OPEN_SPYRO_CD_MUSIC_H

/* HandleMusicCommand's cmd (also queued in g_nPendingMusicCommand). */
#define MUSIC_CMD_PLAY 1 /* start `track` from its first sector */
#define MUSIC_CMD_STOP 2 /* fade out, then pause */
/* same as MUSIC_CMD_STOP (StopAllSoundExceptMask's) */
#define MUSIC_CMD_STOP_4 4
#define MUSIC_CMD_RESUME 8 /* restart from g_dwCdMusicReadHead */

/* g_nCdMusicStreamTimeout, a g_nVblankTickCount deadline (the count restarts
 * at each level load): a track that runs out before it loops; after it, the
 * stream picks another of the level's three and sets the next deadline
 * MUSIC_REPICK_TICKS (8 min at 60 Hz) on. A level's first visit starts with
 * MUSIC_REPICK_FIRST_VISIT_TICKS (12 min). */
#define MUSIC_REPICK_TICKS 0x7080
#define MUSIC_REPICK_FIRST_VISIT_TICKS 0xA8C0

/* g_dwMusicStreamStatus: one state at a time. */
/* CdlReadS issued; the stream tick follows the read head */
#define MUSIC_ST_PLAYING 0x10
#define MUSIC_ST_IDLE 0x40 /* drive free: data reads may run */
/* commands ignored (never set by the game code) */
#define MUSIC_ST_LOCKED 0x80
#define MUSIC_ST_PAUSED 0x100 /* faded out and paused */
/* fading out; g_nLastCdMusicCommand runs at volume 0 */
#define MUSIC_ST_FADING 0x200

#endif /* OPEN_SPYRO_CD_MUSIC_H */
