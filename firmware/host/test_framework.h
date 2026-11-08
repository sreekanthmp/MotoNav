// Minimal test harness: no dependencies, runs on any machine with a C++ compiler.
#pragma once

#include <stdio.h>
#include <string.h>

#include <functional>
#include <string>
#include <vector>

namespace hosttest {

struct TestCase {
  const char* name;
  void (*fn)();
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> r;
  return r;
}

struct Registrar {
  Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

inline int& failures() {
  static int n = 0;
  return n;
}
inline int& checks() {
  static int n = 0;
  return n;
}

}  // namespace hosttest

#define TEST(name)                                         \
  static void test_##name();                               \
  static ::hosttest::Registrar registrar_##name(#name, test_##name); \
  static void test_##name()

#define CHECK(cond)                                                          \
  do {                                                                       \
    ++::hosttest::checks();                                                  \
    if (!(cond)) {                                                           \
      ++::hosttest::failures();                                              \
      printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
    }                                                                        \
  } while (0)

#define CHECK_INT(actual, expected)                                          \
  do {                                                                       \
    ++::hosttest::checks();                                                  \
    const long a_ = static_cast<long>(actual);                               \
    const long e_ = static_cast<long>(expected);                             \
    if (a_ != e_) {                                                          \
      ++::hosttest::failures();                                              \
      printf("    FAIL %s:%d: %s == %ld, expected %ld\n", __FILE__, __LINE__, \
             #actual, a_, e_);                                               \
    }                                                                        \
  } while (0)

#define CHECK_STR(actual, expected)                                          \
  do {                                                                       \
    ++::hosttest::checks();                                                  \
    const char* a_ = (actual);                                               \
    const char* e_ = (expected);                                             \
    if (strcmp(a_, e_) != 0) {                                               \
      ++::hosttest::failures();                                              \
      printf("    FAIL %s:%d: %s == \"%s\", expected \"%s\"\n", __FILE__,     \
             __LINE__, #actual, a_, e_);                                     \
    }                                                                        \
  } while (0)
