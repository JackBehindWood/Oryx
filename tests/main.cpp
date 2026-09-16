// Generates main() and the doctest runtime. Exactly one .cpp in this project
// must define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN before including doctest.h —
// every other test file just includes doctest.h and defines TEST_CASEs.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"