#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

TEST_CASE("FlatHashMap default-constructs empty")
{
    FlatHashMap<std::string, int32_t, 4> map;
    CHECK(map.empty());
    CHECK(map.size() == 0);
    CHECK(map.find("missing") == nullptr);
}

TEST_CASE("FlatHashMap insert_or_assign then find, within inline capacity")
{
    FlatHashMap<std::string, int32_t, 4> map;
    map.insert_or_assign("a", 1);
    map.insert_or_assign("b", 2);

    CHECK(map.size() == 2);
    REQUIRE(map.find("a") != nullptr);
    CHECK(*map.find("a") == 1);
    REQUIRE(map.find("b") != nullptr);
    CHECK(*map.find("b") == 2);
    CHECK(map.find("c") == nullptr);
}

TEST_CASE("FlatHashMap insert_or_assign overwrites an existing key without growing size")
{
    FlatHashMap<std::string, int32_t, 4> map;
    map.insert_or_assign("a", 1);
    map.insert_or_assign("a", 99);

    CHECK(map.size() == 1);
    REQUIRE(map.find("a") != nullptr);
    CHECK(*map.find("a") == 99);
}

TEST_CASE("FlatHashMap grows past inline capacity via rehash without losing or corrupting entries")
{
    FlatHashMap<std::string, int32_t, 4> map; // small N to force multiple rehashes below

    constexpr int32_t kEntryCount = 100;
    for (int32_t i = 0; i < kEntryCount; ++i)
    {
        map.insert_or_assign("key" + oryx::to_string(static_cast<ActionId>(i)), i);
    }

    CHECK(map.size() == static_cast<size_t>(kEntryCount));
    for (int32_t i = 0; i < kEntryCount; ++i)
    {
        const std::string key = "key" + oryx::to_string(static_cast<ActionId>(i));
        REQUIRE(map.find(key) != nullptr);
        CHECK(*map.find(key) == i);
    }
}

TEST_CASE("FlatHashMap::for_each visits every entry exactly once")
{
    FlatHashMap<std::string, int32_t, 4> map;
    map.insert_or_assign("a", 1);
    map.insert_or_assign("b", 2);
    map.insert_or_assign("c", 3);

    int32_t visit_count = 0;
    int32_t sum = 0;
    map.for_each(
        [&](const std::string&, int32_t value)
        {
            ++visit_count;
            sum += value;
        });

    CHECK(visit_count == 3);
    CHECK(sum == 6);
}

TEST_CASE("FlatHashMap::erase removes a key and reports whether it existed")
{
    FlatHashMap<std::string, int32_t, 4> map;
    map.insert_or_assign("a", 1);

    CHECK(map.erase("a"));
    CHECK(map.empty());
    CHECK(map.find("a") == nullptr);
    CHECK_FALSE(map.erase("a"));
}

TEST_CASE("FlatHashMap::erase keeps colliding keys reachable, including across the wrap-around")
{
    // All four keys share home slot 15 in a 16-slot table, so they chain past the end into slots 0-2.
    FlatHashMap<int32_t, int32_t, 16> map;
    for (int32_t key : { 15, 31, 47, 63, 0 })
    {
        map.insert_or_assign(key, key * 10);
    }

    REQUIRE(map.erase(15));

    for (int32_t key : { 31, 47, 63, 0 })
    {
        REQUIRE(map.find(key) != nullptr);
        CHECK(*map.find(key) == key * 10);
    }
    CHECK(map.find(15) == nullptr);
    CHECK(map.size() == 4);
}

TEST_CASE("FlatHashMap::erase leaves every surviving key intact after many shuffled erases")
{
    FlatHashMap<int32_t, int32_t, 4> map;
    constexpr int32_t kEntryCount = 200;
    for (int32_t key = 0; key < kEntryCount; ++key)
    {
        map.insert_or_assign(key * 7, key);
    }

    Random random(3);
    std::vector<int32_t> erased;
    for (int32_t key = 0; key < kEntryCount; ++key)
    {
        if (random.get_bool())
        {
            REQUIRE(map.erase(key * 7));
            erased.push_back(key);
        }
    }

    CHECK(map.size() == static_cast<size_t>(kEntryCount) - erased.size());
    for (int32_t key = 0; key < kEntryCount; ++key)
    {
        bool was_erased = std::find(erased.begin(), erased.end(), key) != erased.end();
        CHECK((map.find(key * 7) == nullptr) == was_erased);
    }
}

TEST_CASE("FlatHashMap::erase then insert_or_assign reuses the freed slot")
{
    FlatHashMap<std::string, int32_t, 4> map;
    map.insert_or_assign("a", 1);
    REQUIRE(map.erase("a"));

    map.insert_or_assign("a", 2);

    REQUIRE(map.find("a") != nullptr);
    CHECK(*map.find("a") == 2);
    CHECK(map.size() == 1);
}
