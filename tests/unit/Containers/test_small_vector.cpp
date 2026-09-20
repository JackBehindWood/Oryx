#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

TEST_CASE("SmallVector default-constructs empty, with inline capacity N")
{
    SmallVector<int32_t, 4> values;
    CHECK(values.empty());
    CHECK(values.size() == 0);
    CHECK(values.capacity() == 4);
}

TEST_CASE("SmallVector push_back within N stays at inline capacity")
{
    SmallVector<int32_t, 4> values;
    values.push_back(10);
    values.push_back(20);
    values.push_back(30);

    CHECK(values.size() == 3);
    CHECK(values.capacity() == 4);
    CHECK(values[0] == 10);
    CHECK(values[1] == 20);
    CHECK(values[2] == 30);
    CHECK(values.front() == 10);
}

TEST_CASE("SmallVector push_back past N spills to a doubled heap capacity without losing elements")
{
    SmallVector<int32_t, 4> values;
    for (int32_t i = 0; i < 4; ++i)
    {
        values.push_back(i);
    }
    CHECK(values.capacity() == 4);

    values.push_back(4); // 5th element - past inline capacity
    CHECK(values.size() == 5);
    CHECK(values.capacity() == 8); // first spill doubles N, not N+1

    for (int32_t i = 0; i < 6; ++i)
    {
        values.push_back(5 + i);
    }
    CHECK(values.size() == 11);
    for (int32_t i = 0; i < 11; ++i)
    {
        CHECK(values[static_cast<size_t>(i)] == i);
    }
}

TEST_CASE("SmallVector supports a move-only element type")
{
    SmallVector<UniquePtr<int32_t>, 2> values;
    values.push_back(create_unique<int32_t>(1));
    values.push_back(create_unique<int32_t>(2));
    values.push_back(create_unique<int32_t>(3)); // forces a spill

    REQUIRE(values.size() == 3);
    CHECK(*values[0] == 1);
    CHECK(*values[1] == 2);
    CHECK(*values[2] == 3);

    SmallVector<UniquePtr<int32_t>, 2> moved = std::move(values);
    REQUIRE(moved.size() == 3);
    CHECK(*moved[2] == 3);
}

TEST_CASE("SmallVector copy construction/assignment produces an independent container")
{
    SmallVector<int32_t, 4> original;
    original.push_back(1);
    original.push_back(2);

    SmallVector<int32_t, 4> copy = original;
    copy.push_back(3);

    CHECK(original.size() == 2);
    CHECK(copy.size() == 3);

    SmallVector<int32_t, 4> assigned;
    assigned = original;
    CHECK(assigned == original);
}

TEST_CASE("SmallVector::assign replaces contents, inline and past N")
{
    SmallVector<int32_t, 2> values;
    values.push_back(99);

    values.assign(2, 7);
    CHECK(values.size() == 2);
    CHECK(values[0] == 7);
    CHECK(values[1] == 7);

    values.assign(5, 3);
    CHECK(values.size() == 5);
    for (int32_t value : values)
    {
        CHECK(value == 3);
    }
}

TEST_CASE("SmallVector supports initializer-list construction and equality")
{
    SmallVector<int32_t, 4> values = { 1, 2, 3 };
    CHECK(values == SmallVector<int32_t, 4>{ 1, 2, 3 });
    CHECK(values != SmallVector<int32_t, 4>{ 1, 2 });
}

TEST_CASE("SmallVector's contiguous iterators work with standard algorithms")
{
    SmallVector<int32_t, 4> values = { 3, 1, 4, 1, 5, 9 };

    CHECK(std::find(values.begin(), values.end(), 4) != values.end());
    CHECK(std::max_element(values.begin(), values.end()) - values.begin() == 5); // "9" at index 5

    int32_t sum = 0;
    for (int32_t value : values)
    {
        sum += value;
    }
    CHECK(sum == 23);
}

TEST_CASE("SmallVector push_back of an element of itself survives the growth it triggers")
{
    SmallVector<int32_t, 2> values{ 7, 8 };
    values.push_back(values[0]);

    CHECK(values.size() == 3);
    CHECK(values[2] == 7);
}

TEST_CASE("SmallVector pop_back and back operate on the last element")
{
    SmallVector<int32_t, 4> values{ 1, 2, 3 };
    CHECK(values.back() == 3);

    values.pop_back();
    CHECK(values.size() == 2);
    CHECK(values.back() == 2);
}
