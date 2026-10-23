/* level_flight_results.h — declarations shared by the flight-level results
 * clones (*_FlightResults_Draw.c, *_FlightResultsMenu_Update.c,
 * *_HandleFlightResults.c), declared once.
 *
 * Each item is spelled identically in every file that includes this header;
 * files that declare any of these names differently keep their own
 * declarations. */

#ifndef OPEN_SPYRO_LEVEL_FLIGHT_RESULTS_H
#define OPEN_SPYRO_LEVEL_FLIGHT_RESULTS_H

/* Declared identically by two or more *FlightResults*.c files; hoisted here. */
extern void AddPrimToOT();
extern char g_abFrameDrawEnv0[];
extern char g_abHudIconActorRecords[];
extern int g_anLevelGemsCollected[];
extern char g_anMenuBackdropGrayClut[];
extern int g_nVsyncFrameEndCount[];
extern char *g_pActiveFrameDrawEnv;
extern unsigned char *g_pLevelSampleBankHeader;
extern char *g_pPrimBufferLimit;
extern char *g_pPrimBufferWriteCursor;
extern char *g_pSpriteRecordBufferTop;

#endif /* OPEN_SPYRO_LEVEL_FLIGHT_RESULTS_H */
