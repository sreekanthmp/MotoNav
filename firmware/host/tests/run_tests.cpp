#include <stdio.h>
#include <string.h>

#include "test_framework.h"

int main(int argc, char** argv) {
  const char* filter = argc > 1 ? argv[1] : nullptr;
  int ran = 0;
  for (const auto& t : hosttest::registry()) {
    if (filter && !strstr(t.name, filter)) continue;
    const int before = hosttest::failures();
    t.fn();
    ++ran;
    printf("[%s] %s\n", hosttest::failures() == before ? " ok " : "FAIL", t.name);
  }
  printf("\n%d tests, %d checks, %d failed checks\n", ran, hosttest::checks(),
         hosttest::failures());
  return hosttest::failures() == 0 ? 0 : 1;
}
