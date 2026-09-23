#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

#ifdef OX_ENABLE_PROFILING

namespace
{

int32_t* volatile g_escape = nullptr;

}

TEST_CASE("OX_PROFILE_SCOPE records one sample per scope exit with consistent min/max/total")
{
    Instrumentation::reset();

    for (int32_t i = 0; i < 3; ++i)
    {
        OX_PROFILE_SCOPE("test::instrumentation_scope");
    }

    const std::unordered_map<std::string_view, ProfileSample>& results = Instrumentation::results();
    REQUIRE(results.count("test::instrumentation_scope") == 1);

    const ProfileSample& sample = results.at("test::instrumentation_scope");
    CHECK(sample.call_count == 3);
    CHECK(sample.min_milliseconds <= sample.max_milliseconds);
    CHECK(sample.total_milliseconds >= sample.max_milliseconds);
}

TEST_CASE("Instrumentation::reset clears every recorded scope")
{
    { OX_PROFILE_SCOPE("test::instrumentation_reset"); }
    Instrumentation::reset();

    CHECK(Instrumentation::results().empty());
}

#ifdef OX_ENABLE_MEMORY_TRACKING
TEST_CASE("ScopeTimer attributes heap allocations made inside the scope to that scope")
{
    Instrumentation::reset();
    {
        ScopeTimer timer("test::allocating_scope");
        int32_t* value = new int32_t(1);
        g_escape = value;
        delete value;
    }

    const ProfileSample& sample = Instrumentation::results().at("test::allocating_scope");
    CHECK(sample.allocation_count == 1);
    // The tracker reports the allocator's usable size, not the requested size, so a small
    // request can round up to the allocator's smallest bucket.
    CHECK(sample.bytes_allocated >= sizeof(int32_t));
}
#endif // OX_ENABLE_MEMORY_TRACKING

#endif // OX_ENABLE_PROFILING
