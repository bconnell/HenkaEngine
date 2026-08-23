#ifndef HENKA_TEST_ASSERTIONS_H
#define HENKA_TEST_ASSERTIONS_H

/* Release optimization must not compile the test suite's assertions out. */
#ifdef NDEBUG
#undef NDEBUG
#endif

#endif
