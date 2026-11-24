/* src/native/host/keymap.c — keyboard key names (keymap.h). */

#include "keymap.h"

#include <ctype.h>
#include <string.h>

static const struct {
  const char *name;
  int key;
} named[] = {
    {"return", KEY_RETURN},
    {"enter", KEY_RETURN},
    {"escape", KEY_ESCAPE},
    {"backspace", KEY_BACKSPACE},
    {"tab", KEY_TAB},
    {"space", KEY_SPACE},
    {"minus", KEY_MINUS},
    {"equals", KEY_EQUALS},
    {"leftbracket", KEY_LEFTBRACKET},
    {"rightbracket", KEY_RIGHTBRACKET},
    {"backslash", KEY_BACKSLASH},
    {"semicolon", KEY_SEMICOLON},
    {"apostrophe", KEY_APOSTROPHE},
    {"grave", KEY_GRAVE},
    {"comma", KEY_COMMA},
    {"period", KEY_PERIOD},
    {"slash", KEY_SLASH},
    {"capslock", KEY_CAPSLOCK},
    {"insert", KEY_INSERT},
    {"home", KEY_HOME},
    {"pageup", KEY_PAGEUP},
    {"delete", KEY_DELETE},
    {"end", KEY_END},
    {"pagedown", KEY_PAGEDOWN},
    {"right", KEY_RIGHT},
    {"left", KEY_LEFT},
    {"down", KEY_DOWN},
    {"up", KEY_UP},
    {"kp_divide", KEY_KP_DIVIDE},
    {"kp_multiply", KEY_KP_MULTIPLY},
    {"kp_minus", KEY_KP_MINUS},
    {"kp_plus", KEY_KP_PLUS},
    {"kp_enter", KEY_KP_ENTER},
    {"kp_period", KEY_KP_PERIOD},
    {"lctrl", KEY_LCTRL},
    {"lshift", KEY_LSHIFT},
    {"lalt", KEY_LALT},
    {"lgui", KEY_LGUI},
    {"rctrl", KEY_RCTRL},
    {"rshift", KEY_RSHIFT},
    {"ralt", KEY_RALT},
    {"rgui", KEY_RGUI},
};

/* "1".."9" or "0" from KEY_1 (or KEY_KP_1): the number row's order. */
static int digit(char c, int one) {
  return c == '0' ? one + 9 : one + (c - '1');
}

int keymap_key(const char *name, int len) {
  char s[16];

  if (len <= 0 || len >= (int)sizeof(s)) {
    return 0;
  }
  for (int i = 0; i < len; i++) {
    s[i] = (char)tolower((unsigned char)name[i]);
  }
  s[len] = '\0';
  if (len == 1 && s[0] >= 'a' && s[0] <= 'z') {
    return KEY_LETTER(s[0]);
  }
  if (len == 1 && isdigit((unsigned char)s[0])) {
    return digit(s[0], KEY_1);
  }
  if (len == 4 && strncmp(s, "kp_", 3) == 0 && isdigit((unsigned char)s[3])) {
    return digit(s[3], KEY_KP_1);
  }
  if (s[0] == 'f' && (len == 2 || len == 3) && isdigit((unsigned char)s[1]) &&
      s[1] != '0' && (len == 2 || isdigit((unsigned char)s[2]))) {
    int n = len == 2 ? s[1] - '0' : (s[1] - '0') * 10 + (s[2] - '0');

    return n >= 1 && n <= 12 ? KEY_F1 + n - 1 : 0;
  }
  for (size_t i = 0; i < sizeof(named) / sizeof(named[0]); i++) {
    if (strcmp(s, named[i].name) == 0) {
      return named[i].key;
    }
  }
  return 0;
}

int keymap_reserved(int key) {
  return key == KEY_ESCAPE || key == KEY_F1 + 3 || key == KEY_F1 + 5 ||
         key == KEY_GRAVE || key == KEY_EQUALS;
}
