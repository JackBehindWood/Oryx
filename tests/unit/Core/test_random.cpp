#include "doctest.h"

#include "Oryx.h"

TEST_CASE("oryx::Random with the same seed produces the same sequence")
{
    oryx::Random a(42);
    oryx::Random b(42);

    for (int i = 0; i < 10; ++i)
    {
        CHECK(a.next_int(0, 1000000) == b.next_int(0, 1000000));
    }
}

TEST_CASE("oryx::Random next_int respects inclusive [min, max] bounds")
{
    oryx::Random random(1);

    for (int i = 0; i < 1000; ++i)
    {
        int64_t value = random.next_int(5, 7);
        CHECK(value >= 5);
        CHECK(value <= 7);
    }
}

TEST_CASE("oryx::Random with different seeds produces different sequences")
{
    oryx::Random a(1);
    oryx::Random b(2);

    bool any_different = false;
    for (int i = 0; i < 20; ++i)
    {
        if (a.next_int(0, 1000000000) != b.next_int(0, 1000000000))
        {
            any_different = true;
            break;
        }
    }

    CHECK(any_different);
}
