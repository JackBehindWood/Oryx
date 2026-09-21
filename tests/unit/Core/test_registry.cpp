#include "doctest.h"

#include "Oryx.h"

#include "unit/Game/DummyGame.h"

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
    DummyRegistry::register_factory("dummy-a", [](const oryx::Params&) { return oryx::create_unique<DummyA>(); });

    oryx::UniquePtr<IDummy> instance = DummyRegistry::create("dummy-a");
    REQUIRE(instance != nullptr);
    CHECK(instance->label() == "A");
}

TEST_CASE("Registry<T>::has reflects registered names")
{
    DummyRegistry::register_factory("dummy-b", [](const oryx::Params&) { return oryx::create_unique<DummyB>(); });

    CHECK(DummyRegistry::has("dummy-b"));
    CHECK_FALSE(DummyRegistry::has("dummy-does-not-exist"));
}

TEST_CASE("Registry<T>::names lists every registered name")
{
    DummyRegistry::register_factory("dummy-c", [](const oryx::Params&) { return oryx::create_unique<DummyA>(); });

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
    static const oryx::Register<IDummy> registrar("dummy-d", [](const oryx::Params&) { return oryx::create_unique<DummyB>(); });

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
                                       [](const oryx::Params&) { return oryx::create_unique<DummyA>(); });
    }

    for (int32_t i = 0; i < 50; ++i)
    {
        CHECK(BigRegistry::has("big-" + oryx::to_string(static_cast<oryx::ActionId>(i))));
    }
}

namespace
{

class ParamDummy : public IDummy
{
public:
    explicit ParamDummy(const oryx::Params& params)
        : m_stones(oryx::get_param<int64_t>(params, "stones"))
        , m_scale(oryx::get_param<double>(params, "scale"))
    {
    }

    std::string label() const override { return "stones=" + oryx::to_string(static_cast<oryx::ActionId>(m_stones)); }
    double scale() const { return m_scale; }

private:
    int64_t m_stones;
    double m_scale;
};

oryx::EntryInfo param_dummy_info()
{
    return { { oryx::int_param("stones", 21, "Pile size"), oryx::double_param("scale", 1.5) }, "Dummy with params" };
}

void register_param_dummy(const std::string& name)
{
    DummyRegistry::register_factory(
        name,
        [](const oryx::Params& params) { return oryx::UniquePtr<IDummy>(oryx::create_unique<ParamDummy>(params)); },
        param_dummy_info());
}

} // namespace

TEST_CASE("Registry<T>::create hands the factory the validated params with defaults filled in")
{
    register_param_dummy("param-dummy-defaults");

    oryx::UniquePtr<IDummy> defaulted = DummyRegistry::create("param-dummy-defaults");
    REQUIRE(defaulted != nullptr);
    CHECK(defaulted->label() == "stones=21");
    CHECK(static_cast<ParamDummy&>(*defaulted).scale() == 1.5);

    oryx::UniquePtr<IDummy> overridden = DummyRegistry::create("param-dummy-defaults", { { "stones", int64_t{ 15 } } });
    REQUIRE(overridden != nullptr);
    CHECK(overridden->label() == "stones=15");
}

TEST_CASE("Registry<T>::create widens an int param to a double param")
{
    register_param_dummy("param-dummy-widen");

    oryx::UniquePtr<IDummy> instance = DummyRegistry::create("param-dummy-widen", { { "scale", int64_t{ 2 } } });
    REQUIRE(instance != nullptr);
    CHECK(static_cast<ParamDummy&>(*instance).scale() == 2.0);
}

TEST_CASE("Registry<T>::create throws a ParamError naming an unknown key")
{
    register_param_dummy("param-dummy-unknown");

    try
    {
        oryx::UniquePtr<IDummy> instance = DummyRegistry::create("param-dummy-unknown", { { "stonez", int64_t{ 3 } } });
        FAIL("expected a ParamError");
    }
    catch (const oryx::ParamError& error)
    {
        CHECK(error.key() == "stonez");
        CHECK(std::string(error.what()).find("stonez") != std::string::npos);
        CHECK(std::string(error.what()).find("param-dummy-unknown") != std::string::npos);
        CHECK(std::string(error.category()) == "param");
    }
}

TEST_CASE("Registry<T>::create throws a ParamError naming a mistyped key")
{
    register_param_dummy("param-dummy-type");

    CHECK_THROWS_WITH_AS(DummyRegistry::create("param-dummy-type", { { "stones", std::string("many") } }),
                         doctest::Contains("'stones'"), oryx::ParamError);
    CHECK_THROWS_AS(DummyRegistry::create("param-dummy-type", { { "stones", true } }), oryx::ParamError);
}

TEST_CASE("Registry<T>::info returns the schema and description stored at registration")
{
    register_param_dummy("param-dummy-info");

    const oryx::EntryInfo* info = DummyRegistry::info("param-dummy-info");
    REQUIRE(info != nullptr);
    CHECK(info->description == "Dummy with params");
    REQUIRE(info->schema.size() == 2);
    CHECK(info->schema[0].name == "stones");
    CHECK(info->schema[0].type == oryx::ParamType::Int);
    CHECK(info->schema[0].description == "Pile size");
    CHECK(DummyRegistry::info("param-dummy-not-registered") == nullptr);
}

TEST_CASE("Registering a name again replaces its factory and info")
{
    DummyRegistry::register_factory("dummy-replaced", [](const oryx::Params&) { return oryx::UniquePtr<IDummy>(oryx::create_unique<DummyA>()); });
    DummyRegistry::register_factory("dummy-replaced", [](const oryx::Params&) { return oryx::UniquePtr<IDummy>(oryx::create_unique<DummyB>()); },
                                    { {}, "second" });

    CHECK(DummyRegistry::create("dummy-replaced")->label() == "B");
    CHECK(DummyRegistry::info("dummy-replaced")->description == "second");
}

TEST_CASE("Registry<T>::unregister_factory removes a name and reports whether it was registered")
{
    DummyRegistry::register_factory("dummy-temporary", [](const oryx::Params&) { return oryx::create_unique<DummyA>(); });

    CHECK(DummyRegistry::unregister_factory("dummy-temporary"));
    CHECK_FALSE(DummyRegistry::has("dummy-temporary"));
    CHECK(DummyRegistry::create("dummy-temporary") == nullptr);
    CHECK_FALSE(DummyRegistry::unregister_factory("dummy-temporary"));
}

TEST_CASE("a factory may register and replace entries, including its own, while it runs")
{
    oryx::GameRegistry::register_factory("reentrant", [](const oryx::Params&) -> oryx::UniquePtr<oryx::IGame>
    {
        for (int32_t i = 0; i < 64; ++i)
        {
            oryx::GameRegistry::register_factory("reentrant-filler-" + std::to_string(i), [](const oryx::Params&) -> oryx::UniquePtr<oryx::IGame> { return nullptr; });
        }
        oryx::GameRegistry::register_factory("reentrant", [](const oryx::Params&) -> oryx::UniquePtr<oryx::IGame> { return nullptr; });
        return oryx::create_unique<oryx::test::DummyGame>(3);
    });

    oryx::UniquePtr<oryx::IGame> game = oryx::GameRegistry::create("reentrant");
    CHECK(game != nullptr);
    CHECK(oryx::GameRegistry::create("reentrant") == nullptr);

    oryx::GameRegistry::unregister_factory("reentrant");
    for (int32_t i = 0; i < 64; ++i)
    {
        oryx::GameRegistry::unregister_factory("reentrant-filler-" + std::to_string(i));
    }
}
