#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

TEST_CASE("derive_seed() is a pure function of its arguments")
{
    CHECK(derive_seed(7, "a vs b", SeedRole::Game) == derive_seed(7, "a vs b", SeedRole::Game));
    CHECK(hash_string("a vs b") == hash_string("a vs b"));
}

TEST_CASE("derive_seed() is distinct per master, key, role, seat and repeat")
{
    std::set<uint64_t> seeds;
    for (uint64_t master : { 1ULL, 2ULL })
    {
        for (const char* key : { "a", "b" })
        {
            for (SeedRole role : { SeedRole::Game, SeedRole::Strategy, SeedRole::Experiment })
            {
                for (uint32_t seat = 0; seat < 3; ++seat)
                {
                    for (uint32_t repeat = 0; repeat < 3; ++repeat)
                    {
                        seeds.insert(derive_seed(master, key, role, seat, repeat));
                    }
                }
            }
        }
    }
    CHECK(seeds.size() == 2U * 2U * 3U * 3U * 3U);
}

TEST_CASE("derive_seed() for one key is unaffected by which other keys are derived, or in what order")
{
    uint64_t alone = derive_seed(42, "b", SeedRole::Strategy, 1, 2);

    for (const char* key : { "c", "a", "d" })
    {
        (void)derive_seed(42, key, SeedRole::Strategy, 1, 2);
    }

    CHECK(derive_seed(42, "b", SeedRole::Strategy, 1, 2) == alone);
}

TEST_CASE("hash_string() is FNV-1a-64, fixed across platforms")
{
    CHECK(hash_string("") == 0xCBF29CE484222325ULL);
    CHECK(hash_string("a") == 0xAF63DC4C8601EC8CULL);
}
