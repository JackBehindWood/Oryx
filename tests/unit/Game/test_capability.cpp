#include "doctest.h"

#include "DummyGame.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

// Mock capability, provided explicitly via Context::provide() - not a
// State subclass.
class IMockCapability
{
public:
    virtual ~IMockCapability() = default;
    virtual int32_t mock_value() const = 0;
};

class MockCapability : public IMockCapability
{
public:
    int32_t mock_value() const override { return 42; }
};

class MockStrategy : public IStrategy
{
public:
    ActionId decide(const Context& context) override
    {
        const IMockCapability* capability = context.get<IMockCapability>();
        return capability != nullptr ? static_cast<ActionId>(capability->mock_value()) : INVALID_ACTION;
    }

    [[nodiscard]] std::vector<std::type_index> required_capabilities() const override
    {
        return { std::type_index(typeid(IMockCapability)) };
    }
};

} // namespace

TEST_CASE("Context::get returns the capability that was explicitly provided")
{
    DummyState state(10);
    Context context(state);
    MockCapability capability;
    context.provide<IMockCapability>(&capability);

    IMockCapability* resolved = context.get<IMockCapability>();
    REQUIRE(resolved != nullptr);
    CHECK(resolved->mock_value() == 42);
}

TEST_CASE("Context::get returns nullptr when nothing was provided for that capability")
{
    DummyState state(10);
    Context context(state);

    CHECK(context.get<IMockCapability>() == nullptr);
}

TEST_CASE("A strategy using an unavailable capability degrades instead of crashing")
{
    DummyState state(10);
    Context context(state);

    MockStrategy strategy;
    CHECK(strategy.decide(context) == INVALID_ACTION);
}

TEST_CASE("Context::has_capability reflects only what has been explicitly provided")
{
    DummyState state(10);
    Context context(state);
    std::type_index mock_capability = std::type_index(typeid(IMockCapability));

    CHECK_FALSE(context.has_capability(mock_capability));

    MockCapability capability;
    context.provide<IMockCapability>(&capability);

    CHECK(context.has_capability(mock_capability));
}

TEST_CASE("A strategy's required_capabilities can be validated against a Context before decide() runs")
{
    DummyState state(10);
    Context unprovisioned(state);
    Context provisioned(state);
    MockCapability capability;
    provisioned.provide<IMockCapability>(&capability);

    MockStrategy strategy;

    bool unprovisioned_ok = true;
    for (const std::type_index& required : strategy.required_capabilities())
    {
        unprovisioned_ok = unprovisioned_ok && unprovisioned.has_capability(required);
    }
    CHECK_FALSE(unprovisioned_ok);

    bool provisioned_ok = true;
    for (const std::type_index& required : strategy.required_capabilities())
    {
        provisioned_ok = provisioned_ok && provisioned.has_capability(required);
    }
    CHECK(provisioned_ok);
}
