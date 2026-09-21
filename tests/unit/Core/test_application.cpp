#include "doctest.h"

#include "unit/Game/DummyGame.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

class ThrowingLayer : public Layer
{
public:
    ThrowingLayer()
        : Layer("ThrowingLayer")
    {
    }

    void update() override { throw Error("layer failure"); }
};

class ThrowingStrategy : public IStrategy
{
public:
    ActionId decide(const Context&) override { throw Error("strategy failure"); }
};

class ClosingApp : public Application
{
public:
    ClosingApp()
        : Application({})
    {
        push_layer<ThrowingLayer>();
    }

    int32_t disabled_count = 0;

protected:
    void on_layer_disabled(Layer&, std::string_view) override
    {
        ++disabled_count;
        close(1);
    }
};

class SimulatingApp : public Application
{
public:
    SimulatingApp()
        : Application({})
    {
        push_layer<SimulationLayer>(false);

        SmallVector<UniquePtr<IStrategy>, 2> strategies;
        strategies.push_back(create_unique<ThrowingStrategy>());
        strategies.push_back(create_unique<ThrowingStrategy>());
        StartSimulationEvent event(create_unique<DummyGame>(5), std::move(strategies), 1, nullptr, false);
        post_event(event);
    }

protected:
    void on_layer_disabled(Layer&, std::string_view) override { close(1); }
};

} // namespace

TEST_CASE("Application::close records the first non-zero exit code")
{
    Application app({});
    CHECK(app.exit_code() == 0);

    app.close();
    CHECK(app.exit_code() == 0);

    app.close(2);
    app.close(0);
    app.close(3);
    CHECK(app.exit_code() == 2);
}

TEST_CASE("An Application that closes from on_layer_disabled stops running after a layer fails")
{
    ClosingApp app;
    app.run();

    CHECK(app.disabled_count == 1);
    CHECK(app.exit_code() == 1);
}

TEST_CASE("A strategy that throws inside SimulationLayer ends the Application")
{
    SimulatingApp app;
    app.run();

    CHECK(app.exit_code() == 1);
}
