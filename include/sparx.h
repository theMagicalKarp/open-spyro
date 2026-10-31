/* sparx.h — Sparx's health (g_nSparxHealth / SPY.sparxHealth). Each hit costs
 * one level; a hit at SPARX_GONE takes it to SPARX_FATAL and Spyro dies
 * (DispatchSpyroPadStateChanges). Eating a butterfly restores one. */

#ifndef OPEN_SPYRO_SPARX_H
#define OPEN_SPYRO_SPARX_H

enum SparxHealth {
  SPARX_FATAL = -1, /* the fatal hit was taken */
  SPARX_GONE = 0,   /* no Sparx: the next hit is fatal */
  SPARX_GREEN = 1,
  SPARX_BLUE = 2,
  SPARX_GOLD = 3    /* full health (a new game) */
};

#endif /* OPEN_SPYRO_SPARX_H */
