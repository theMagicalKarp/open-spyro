/* native/tests/test_main.c — runs every registered TEST (test.h).
 *
 *   native_tests [FILTER]   only the tests whose name contains FILTER */

#include <stdio.h>
#include <string.h>

#include "test.h"

static TestCase *head, **tail = &head;

void test_register(TestCase *t) {
  *tail = t;
  tail = &t->next;
}

int main(int argc, char **argv) {
  const char *filter = argc > 1 ? argv[1] : NULL;
  int run = 0, failed = 0;

  for (TestCase *t = head; t != NULL; t = t->next) {
    int f = 0;

    if (filter != NULL && strstr(t->name, filter) == NULL) {
      continue;
    }
    t->fn(&f);
    run++;
    if (f) {
      fprintf(stderr, "FAIL %s\n", t->name);
      failed++;
    }
  }
  printf("%d tests, %d failed\n", run, failed);
  return failed != 0;
}
