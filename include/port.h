/* port.h — macros that let one source text serve the PS1 build and the native
 * port. Included from types.h.
 *
 * Every macro here expands, for the PS1 build, to exactly the text it
 * replaces, so the PS1 preprocessed output is unchanged (`open-spyro
 * neutral`). The native build (wasm32 clang, `__psyz` defined) gets a plain C
 * equivalent. */

#ifndef OPEN_SPYRO_PORT_H
#define OPEN_SPYRO_PORT_H

#ifdef __psyz

/* A register pin only steers gcc 2.7.2's allocator; natively it is an
 * ordinary local. */
#define REG_PIN(type, name, reg) type name

/* The PS1 build declares these globals plain in some headers and volatile in
 * others (each TU sees one form). clang rejects the mix, so natively every
 * declaration is volatile. */
#define PORT_VOLATILE volatile

#else

/* `register type name asm(reg)`: the variable lives in hard register `reg`
 * ("$2" = v0, ...) for its whole life (cookbook A374). */
#define REG_PIN(type, name, reg) register type name asm(reg)

#define PORT_VOLATILE

#endif

#endif /* OPEN_SPYRO_PORT_H */
