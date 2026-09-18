#include "doctest.h"

#include "Oryx.h"

namespace
{

class IDummy
{
public:
    virtual ~IDummy() = default;
    virtual std::string label() const = 0;
};

class DummyA : public IDummy
{
public:
    std::string label() const override { return "A"; }
};

class DummyB : public IDummy
{
public:
    std::string label() const override { return "B"; }
};

using DummyRegistry = oryx::Registry<IDummy>;

} // namespace

TEST_CASE("Registry<T>::create constructs the type registered under a given name")
{
    DummyRegistry::register_factory("dummy-a", []() { return oryx::create_unique<DummyA>(); });

    oryx::UniquePtr<IDummy> instance = DummyRegistry::create("dummy-a");
    REQUIRE(instance != nullptr);
    CHECK(instance->label() == "A");
}

TEST_CASE("Registry<T>::has reflects registered names")
{
    DummyRegistry::register_factory("dummy-b", []() { return oryx::create_unique<DummyB>(); });

    CHECK(DummyRegistry::has("dummy-b"));
    CHECK_FALSE(DummyRegistry::has("dummy-does-not-exist"));
}

TEST_CASE("Registry<T>::names lists every registered name")
{
    DummyRegistry::register_factory("dummy-c", []() { return oryx::create_unique<DummyA>(); });

    std::vector<std::string> names = DummyRegistry::names();
    CHECK(std::find(names.begin(), names.end(), "dummy-c") != names.end());
}

TEST_CASE("Registry<T>::create returns nullptr for an unregistered name")
{
    oryx::UniquePtr<IDummy> instance = DummyRegistry::create("dummy-unregistered");
    CHECK(instance == nullptr);
}

TEST_CASE("Register<T>'s constructor performs the registration itself, as OX_REGISTER_GAME/OX_REGISTER_STRATEGY expand to")
{
    static const oryx::Register<IDummy> registrar("dummy-d", []() { return oryx::create_unique<DummyB>(); });

    REQUIRE(DummyRegistry::has("dummy-d"));
    oryx::UniquePtr<IDummy> instance = DummyRegistry::create("dummy-d");
    REQUIRE(instance != nullptr);
    CHECK(instance->label() == "B");
}

TEST_CASE("Registry<T> keeps every entry correct past its inline capacity")
{
    using BigRegistry = oryx::Registry<IDummy>;

    for (int32_t i = 0; i < 50; ++i)
    {
        BigRegistry::register_factory("big-" + oryx::to_string(static_cast<oryx::ActionId>(i)),
                                       []() { return oryx::create_unique<DummyA>(); });
    }

    for (int32_t i = 0; i < 50; ++i)
    {
        CHECK(BigRegistry::has("big-" + oryx::to_string(static_cast<oryx::ActionId>(i))));
    }
}
