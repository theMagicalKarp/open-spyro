/* libcd.h — the PSY-Q libcd command codes and mode bits this game passes to
 * CdControl / CdControlB / CdControlF / CdRead (standard SDK values). */

#ifndef OPEN_SPYRO_LIBCD_H
#define OPEN_SPYRO_LIBCD_H

/* CdControl commands */
#define CdlSetloc 0x02
#define CdlStop 0x08
#define CdlPause 0x09
#define CdlSetfilter 0x0D
#define CdlSetmode 0x0E
#define CdlGetlocP 0x11
#define CdlReadS 0x1B

/* CdlSetmode / CdRead mode bits */
#define CdlModeSpeed 0x80 /* double speed */
#define CdlModeRT 0x40    /* ADPCM (XA audio) play */
#define CdlModeSF 0x08    /* subheader filter (CdlSetfilter's file/channel) */

#endif /* OPEN_SPYRO_LIBCD_H */
