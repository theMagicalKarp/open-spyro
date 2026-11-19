/* native/tests/test.h — a minimal unit-test harness for the native host code.
 *
 *   TEST(name) { CHECK(cond); CHECK_INT(got, want); CHECK_STR(got, want); }
 *
 * Every TEST registers itself (a constructor), so test_main.c runs them all
 * without a list. A failed check reports file:line and ends that test. */

#ifndef OPEN_SPYRO_NATIVE_TEST_H
#define OPEN_SPYRO_NATIVE_TEST_H

#include <stdio.h>
#include <string.h>

typedef struct TestCase {
  const char *name;
  void (*fn)(int *failed);
  struct TestCase *next;
} TestCase;

void test_register(TestCase *t);

#define TEST(name)                                                             \
  static void test_##name(int *test_failed_);                                  \
  static TestCase test_case_##name = {#name, test_##name, NULL};               \
  __attribute__((constructor)) static void test_reg_##name(void) {             \
    test_register(&test_case_##name);                                          \
  }                                                                            \
  static void test_##name(int *test_failed_)

#define TEST_FAIL_(...)                                                        \
  do {                                                                         \
    fprintf(stderr, "  %s:%d: ", __FILE__, __LINE__);                          \
    fprintf(stderr, __VA_ARGS__);                                              \
    fputc('\n', stderr);                                                       \
    *test_failed_ = 1;                                                         \
    return;                                                                    \
  } while (0)

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      TEST_FAIL_("CHECK(%s) failed", #cond);                                   \
    }                                                                          \
  } while (0)

#define CHECK_INT(got, want)                                                   \
  do {                                                                         \
    long long g_ = (long long)(got), w_ = (long long)(want);                   \
    if (g_ != w_) {                                                            \
      TEST_FAIL_("%s == %lld, want %lld", #got, g_, w_);                       \
    }                                                                          \
  } while (0)

#define CHECK_STR(got, want)                                                   \
  do {                                                                         \
    const char *g_ = (got), *w_ = (want);                                      \
    if (g_ == NULL || w_ == NULL ? g_ != w_ : strcmp(g_, w_) != 0) {           \
      TEST_FAIL_("%s == \"%s\", want \"%s\"", #got, g_ ? g_ : "(null)",        \
                 w_ ? w_ : "(null)");                                          \
    }                                                                          \
  } while (0)

#endif
