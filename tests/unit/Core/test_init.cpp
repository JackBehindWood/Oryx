#include "doctest.h"

#include "Oryx.h"

TEST_CASE("The test main initialises Oryx")
{
    CHECK(oryx::is_initialised());
}

TEST_CASE("init can be called repeatedly")
{
    CHECK_NOTHROW(oryx::init());
    CHECK_NOTHROW(oryx::init());
    CHECK(oryx::is_initialised());
}
