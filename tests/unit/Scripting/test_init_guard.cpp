#include "doctest.h"

#include "unit/TestLogCapture.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

int32_t add(int32_t a, int32_t b)
{
    return a + b;
}

int32_t g_calls = 0;

void count_call(const std::string& text)
{
    g_calls += static_cast<int32_t>(text.size());
}

} // namespace

TEST_CASE("Guarded forwards arguments and the result once Oryx is initialised")
{
    int32_t (*guarded)(int32_t, int32_t) = OX_GUARDED_FUNC(add, "add");
    CHECK(guarded(2, 3) == 5);
}

TEST_CASE("Guarded throws an Error naming the function and does not run it before init")
{
    g_calls = 0;
    void (*guarded)(const std::string&) = OX_GUARDED_FUNC(count_call, "test.count_call");

    UninitialisedScope uninitialised;
    CHECK_THROWS_WITH_AS(guarded("abc"), "test.count_call() was called before Oryx was initialised", Error);
    CHECK(g_calls == 0);
}

TEST_CASE("throw_not_initialised throws a plain Error")
{
    CHECK_THROWS_WITH_AS(throw_not_initialised("thing"), "thing() was called before Oryx was initialised", Error);
}
