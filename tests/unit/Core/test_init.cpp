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

TEST_CASE("shutdown runs the registered hooks once, in reverse registration order")
{
    std::vector<int32_t> order;
    oryx::register_shutdown_hook([&order] { order.push_back(1); });
    oryx::register_shutdown_hook([&order] { order.push_back(2); });
    oryx::register_shutdown_hook([&order] { order.push_back(3); });

    oryx::shutdown();
    CHECK(order == std::vector<int32_t>{ 3, 2, 1 });

    oryx::shutdown();
    CHECK(order.size() == 3);
}

TEST_CASE("a hook that throws does not stop the remaining hooks")
{
    std::vector<int32_t> order;
    oryx::register_shutdown_hook([&order] { order.push_back(1); });
    oryx::register_shutdown_hook([] { throw oryx::Error("hook failure"); });
    oryx::register_shutdown_hook([] { throw std::runtime_error("plain failure"); });
    oryx::register_shutdown_hook([&order] { order.push_back(4); });

    CHECK_NOTHROW(oryx::shutdown());
    CHECK(order == std::vector<int32_t>{ 4, 1 });
}

TEST_CASE("a hook registered while shutdown runs waits for the next shutdown")
{
    std::vector<int32_t> order;
    oryx::register_shutdown_hook([&order]
    {
        order.push_back(1);
        oryx::register_shutdown_hook([&order] { order.push_back(2); });
    });

    oryx::shutdown();
    CHECK(order == std::vector<int32_t>{ 1 });

    oryx::shutdown();
    CHECK(order == std::vector<int32_t>{ 1, 2 });
}
