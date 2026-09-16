#include "doctest.h"

#include "Oryx.h"

TEST_CASE("Oryx version constants are non-negative") 
{
    CHECK(oryx::VERSION_MAJOR >= 0);
    CHECK(oryx::VERSION_MINOR >= 0);
    CHECK(oryx::VERSION_PATCH >= 0);
}

TEST_CASE("Oryx foundation links successfully") {
    // Reaching this point means the Tests executable linked against the
    // Oryx static library correctly.
    REQUIRE(true);
}
