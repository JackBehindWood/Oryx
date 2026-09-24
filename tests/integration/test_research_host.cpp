#include "doctest.h"

#include "unit/Python/PythonTestSupport.h"

using namespace oryx;
using namespace oryx::test;

#ifdef OX_ENABLE_PYTHON

namespace
{

std::string expected_version()
{
    return std::to_string(VERSION_MAJOR) + "." + std::to_string(VERSION_MINOR) + "." + std::to_string(VERSION_PATCH);
}

} // namespace

TEST_SUITE("integration")
{

TEST_CASE("research host: the embedded host has __version__ but no init()")
{
    std::string output = run_oryx_script("mark(str(hasattr(oryx, 'init')) + ' ' + oryx.__version__)\n");

    CHECK(output == "False " + expected_version());
}

TEST_CASE("research host: oryx.__version__ matches the C++ version")
{
    if (!can_run_python_extension())
    {
        return;
    }
    CHECK(run_python("import oryx, sys; sys.exit(0 if oryx.__version__ == '" + expected_version() + "' else 1)") == 0);
}

} // TEST_SUITE("integration")

#endif // OX_ENABLE_PYTHON
