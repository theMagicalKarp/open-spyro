/* level_id.h — level ids (g_nActiveLevelId, g_nCurrentLevelId).
 *
 * A level id is world * 10 + n: n = 0 is the world's home, 1..4 its
 * levels (4 the boss in the first five worlds) and 5 its flight level,
 * except in Gnasty's world (60..64), which has no flight level.
 * ApplyPerLevelGlobalsTable maps ids 10..64 onto the level overlays
 * Level0..Level34 in this order; ids 0..9 and 99 load overlays not mapped
 * from WAD.WAD. */

#ifndef OPEN_SPYRO_LEVEL_ID_H
#define OPEN_SPYRO_LEVEL_ID_H

enum LevelId {
  LEVEL_ARTISANS = 10,
  LEVEL_STONE_HILL = 11,
  LEVEL_DARK_HOLLOW = 12,
  LEVEL_TOWN_SQUARE = 13,
  LEVEL_TOASTY = 14,
  LEVEL_SUNNY_FLIGHT = 15,
  LEVEL_PEACE_KEEPERS = 20,
  LEVEL_DRY_CANYON = 21,
  LEVEL_CLIFF_TOWN = 22,
  LEVEL_ICE_CAVERN = 23,
  LEVEL_DOCTOR_SHEMP = 24,
  LEVEL_NIGHT_FLIGHT = 25,
  LEVEL_MAGIC_CRAFTERS = 30,
  LEVEL_ALPINE_RIDGE = 31,
  LEVEL_HIGH_CAVES = 32,
  LEVEL_WIZARD_PEAK = 33,
  LEVEL_BLOWHARD = 34,
  LEVEL_CRYSTAL_FLIGHT = 35,
  LEVEL_BEAST_MAKERS = 40,
  LEVEL_TERRACE_VILLAGE = 41,
  LEVEL_MISTY_BOG = 42,
  LEVEL_TREE_TOPS = 43,
  LEVEL_METALHEAD = 44,
  LEVEL_WILD_FLIGHT = 45,
  LEVEL_DREAM_WEAVERS = 50,
  LEVEL_DARK_PASSAGE = 51,
  LEVEL_LOFTY_CASTLE = 52,
  LEVEL_HAUNTED_TOWERS = 53,
  LEVEL_JACQUES = 54,
  LEVEL_ICY_FLIGHT = 55,
  LEVEL_GNORC_GNEXUS = 60,
  LEVEL_GNORC_COVE = 61,
  LEVEL_TWILIGHT_HARBOR = 62,
  LEVEL_GNASTY_GNORC = 63,
  LEVEL_GNASTYS_LOOT = 64,
};

#endif /* OPEN_SPYRO_LEVEL_ID_H */
