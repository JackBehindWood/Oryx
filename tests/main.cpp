// Generates the doctest runtime; exactly one .cpp in this project must
// define DOCTEST_CONFIG_IMPLEMENT — every other test file just includes
// doctest.h and defines TEST_CASEs. A custom main() (instead of
// DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN) is needed so oryx::init() runs first -
// otherwise any OX_CORE_*/OX_* logging exercised by a test (e.g. Simulation
// integration tests) dereferences a null logger, since this binary has its own
// main() instead of EntryPoint.h's.
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include "Oryx.h"

int main(int argc, char** argv)
{
    oryx::init();

    doctest::Context context;
    context.applyCommandLine(argc, argv);

    int result = context.run();
    if (context.shouldExit())
    {
        return result;
    }

    return result;
}
