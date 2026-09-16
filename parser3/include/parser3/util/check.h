#pragma once

#include <cstdio>
#include <cstdlib>

// Always-on fatal error (used for invariants that must hold in every build).
#define VERA_FAIL(Message)                                                                                             \
    do {                                                                                                               \
        std::fprintf(stderr, "%s:%d: fatal: %s\n", __FILE__, __LINE__, Message);                                       \
        std::abort();                                                                                                  \
    } while (false)

// Assert-like check: compiled in for non-NDEBUG builds, erased otherwise.
#if defined(NDEBUG)
#define VERA_CHECK(Condition, Message) ((void)0)
#else
#define VERA_CHECK(Condition, Message)                                                                                 \
    do {                                                                                                               \
        if (!(Condition)) {                                                                                            \
            VERA_FAIL(Message);                                                                                        \
        }                                                                                                              \
    } while (false)
#endif
