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
