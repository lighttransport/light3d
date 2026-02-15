#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

// Minimal test framework — no external dependencies.
// Usage:
//   TEST(name) { ASSERT(...); ASSERT_NEAR(...); }
//   int main() { RUN_TESTS(); }

static int gTestsPassed = 0;
static int gTestsFailed = 0;
static int gAssertsFailed = 0;

struct TestEntry {
    const char* name;
    void (*fn)();
    TestEntry* next;
};
static TestEntry* gTestHead = nullptr;
static TestEntry** gTestTail = &gTestHead;

struct TestRegistrar {
    TestRegistrar(const char* name, void (*fn)()) {
        static TestEntry pool[512];
        static int poolIdx = 0;
        TestEntry* e = &pool[poolIdx++];
        e->name = name;
        e->fn = fn;
        e->next = nullptr;
        *gTestTail = e;
        gTestTail = &e->next;
    }
};

#define TEST(name)                                                   \
    static void test_##name();                                       \
    static TestRegistrar reg_##name(#name, test_##name);             \
    static void test_##name()

#define ASSERT(expr)                                                 \
    do {                                                             \
        if (!(expr)) {                                               \
            std::fprintf(stderr, "  FAIL: %s:%d: %s\n",             \
                         __FILE__, __LINE__, #expr);                 \
            gAssertsFailed++;                                        \
            return;                                                  \
        }                                                            \
    } while (0)

#define ASSERT_NEAR(a, b, eps)                                       \
    do {                                                             \
        if (std::fabs((a) - (b)) > (eps)) {                         \
            std::fprintf(stderr, "  FAIL: %s:%d: %s ~= %s "        \
                         "(%.9g vs %.9g, eps=%.9g)\n",              \
                         __FILE__, __LINE__, #a, #b,                \
                         static_cast<double>(a),                     \
                         static_cast<double>(b),                     \
                         static_cast<double>(eps));                  \
            gAssertsFailed++;                                        \
            return;                                                  \
        }                                                            \
    } while (0)

#define ASSERT_EQ(a, b)                                              \
    do {                                                             \
        if ((a) != (b)) {                                            \
            std::fprintf(stderr, "  FAIL: %s:%d: %s == %s\n",      \
                         __FILE__, __LINE__, #a, #b);               \
            gAssertsFailed++;                                        \
            return;                                                  \
        }                                                            \
    } while (0)

#define RUN_TESTS()                                                  \
    do {                                                             \
        for (TestEntry* e = gTestHead; e; e = e->next) {            \
            gAssertsFailed = 0;                                      \
            e->fn();                                                 \
            if (gAssertsFailed == 0) {                               \
                gTestsPassed++;                                      \
                std::printf("  PASS: %s\n", e->name);              \
            } else {                                                 \
                gTestsFailed++;                                      \
            }                                                        \
        }                                                            \
        std::printf("\n%d passed, %d failed\n",                     \
                    gTestsPassed, gTestsFailed);                     \
        return gTestsFailed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;      \
    } while (0)
