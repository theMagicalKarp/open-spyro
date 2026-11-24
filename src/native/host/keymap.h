/* src/native/host/keymap.h — keyboard key names for input.keyboard.* and the
 * keys pad 1's buttons default to (keymap.c).
 *
 * SDL-free, so config.c and the host unit tests can use it: the KEY_ values
 * are USB HID usage ids, which SDL_Scancode takes over unchanged (main.c
 * checks a few). */

#ifndef OPEN_SPYRO_NATIVE_KEYMAP_H
#define OPEN_SPYRO_NATIVE_KEYMAP_H

/* The keys one pad button can be bound to; unused slots are 0. */
enum { KEYMAP_KEYS = 4 };

/* The keys bindings can name, as SDL_Scancode values. */
enum {
  KEY_A = 4,  /* .. KEY_Z = 29 */
  KEY_1 = 30, /* .. KEY_9 = 38 */
  KEY_0 = 39,
  KEY_RETURN,
  KEY_ESCAPE,
  KEY_BACKSPACE,
  KEY_TAB,
  KEY_SPACE,
  KEY_MINUS,
  KEY_EQUALS,
  KEY_LEFTBRACKET,
  KEY_RIGHTBRACKET,
  KEY_BACKSLASH,
  KEY_SEMICOLON = 51,
  KEY_APOSTROPHE,
  KEY_GRAVE,
  KEY_COMMA,
  KEY_PERIOD,
  KEY_SLASH,
  KEY_CAPSLOCK,
  KEY_F1 = 58, /* .. KEY_F12 = 69 */
  KEY_INSERT = 73,
  KEY_HOME,
  KEY_PAGEUP,
  KEY_DELETE,
  KEY_END,
  KEY_PAGEDOWN,
  KEY_RIGHT,
  KEY_LEFT,
  KEY_DOWN,
  KEY_UP,
  KEY_KP_DIVIDE = 84,
  KEY_KP_MULTIPLY,
  KEY_KP_MINUS,
  KEY_KP_PLUS,
  KEY_KP_ENTER,
  KEY_KP_1, /* .. KEY_KP_9 = 97 */
  KEY_KP_0 = 98,
  KEY_KP_PERIOD,
  KEY_LCTRL = 224,
  KEY_LSHIFT,
  KEY_LALT,
  KEY_LGUI,
  KEY_RCTRL,
  KEY_RSHIFT,
  KEY_RALT,
  KEY_RGUI,
};
#define KEY_LETTER(c) (KEY_A + ((c) - 'a'))

/* Pad 1's keyboard bindings, in PsyZ's button order (config.c button_names):
   DuckStation's layout, with L2 / R2 (the camera turn) by the face buttons
   and Space as a second Cross (jump). L3 and R3 are unbound. */
#define KEYMAP_DEFAULT                                                         \
  {                                                                            \
      {KEY_LETTER('u')},            /* l2 */                                   \
      {KEY_LETTER('o')},            /* r2 */                                   \
      {KEY_LETTER('q')},            /* l1 */                                   \
      {KEY_LETTER('e')},            /* r1 */                                   \
      {KEY_LETTER('i')},            /* triangle */                             \
      {KEY_LETTER('l')},            /* circle */                               \
      {KEY_LETTER('k'), KEY_SPACE}, /* cross */                                \
      {KEY_LETTER('j')},            /* square */                               \
      {KEY_BACKSPACE},              /* select */                               \
      {0},                          /* l3 */                                   \
      {0},                          /* r3 */                                   \
      {KEY_RETURN},                 /* start */                                \
      {KEY_LETTER('w'), KEY_UP},    /* up */                                   \
      {KEY_LETTER('d'), KEY_RIGHT}, /* right */                                \
      {KEY_LETTER('s'), KEY_DOWN},  /* down */                                 \
      {KEY_LETTER('a'), KEY_LEFT},  /* left */                                 \
  }

/* The key named NAME ("k", "space", "f1", "kp_4", "lshift"; any case), or
   0 if there is none. */
int keymap_key(const char *name, int len);

/* Whether KEY is taken by the host (Escape quits, F4 fullscreen, F6 the VRAM
   view, ` the overlay, = the resolution scale), so no button can have it. */
int keymap_reserved(int key);

#endif
