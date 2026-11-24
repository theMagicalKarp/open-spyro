/* src/native/host/keymap_test.c — unit tests for keymap.c (make native-test).
 */

#include "keymap.h"
#include "test.h"

TEST(keymap_letters_digits_and_function_keys) {
  CHECK_INT(keymap_key("a", 1), KEY_A);
  CHECK_INT(keymap_key("Z", 1), KEY_A + 25);
  CHECK_INT(keymap_key("1", 1), KEY_1);
  CHECK_INT(keymap_key("9", 1), KEY_1 + 8);
  CHECK_INT(keymap_key("0", 1), KEY_0);
  CHECK_INT(keymap_key("kp_1", 4), KEY_KP_1);
  CHECK_INT(keymap_key("kp_0", 4), KEY_KP_0);
  CHECK_INT(keymap_key("f1", 2), KEY_F1);
  CHECK_INT(keymap_key("F12", 3), KEY_F1 + 11);
}

TEST(keymap_named_keys) {
  CHECK_INT(keymap_key("space", 5), KEY_SPACE);
  CHECK_INT(keymap_key("Return", 6), KEY_RETURN);
  CHECK_INT(keymap_key("enter", 5), KEY_RETURN);
  CHECK_INT(keymap_key("up", 2), KEY_UP);
  CHECK_INT(keymap_key("lshift", 6), KEY_LSHIFT);
  CHECK_INT(keymap_key("rgui", 4), KEY_RGUI);
  CHECK_INT(keymap_key("kp_enter", 8), KEY_KP_ENTER);
  /* Only the first LEN characters count. */
  CHECK_INT(keymap_key("space,k", 5), KEY_SPACE);
}

TEST(keymap_rejects_others) {
  CHECK_INT(keymap_key("", 0), 0);
  CHECK_INT(keymap_key("f0", 2), 0);
  CHECK_INT(keymap_key("f13", 3), 0);
  CHECK_INT(keymap_key("f01", 3), 0);
  CHECK_INT(keymap_key("kp_", 3), 0);
  CHECK_INT(keymap_key("jump", 4), 0);
  CHECK_INT(keymap_key("aa", 2), 0);
  CHECK_INT(keymap_key("verylongkeynamethatoverflows", 28), 0);
}

TEST(keymap_reserved_keys) {
  CHECK(keymap_reserved(KEY_ESCAPE));
  CHECK(keymap_reserved(keymap_key("f4", 2)));
  CHECK(keymap_reserved(keymap_key("f6", 2)));
  CHECK(keymap_reserved(KEY_GRAVE));
  CHECK(keymap_reserved(KEY_EQUALS));
  CHECK(!keymap_reserved(keymap_key("f5", 2)));
  CHECK(!keymap_reserved(KEY_SPACE));
}
