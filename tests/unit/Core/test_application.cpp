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

    void update(double) override { throw Error("layer failure"); }
};

class CloseCountingLayer : public Layer
{
public:
    CloseCountingLayer()
        : Layer("CloseCountingLayer")
    {
    }

    void update(double) override
    {
        ++updates;
        if (updates == 2)
        {
            Application::Get().close(2);
            Application::Get().close(3);
        }
    }

    void event(Event& event) override
    {
        EventDispatcher dispatcher(event);
        dispatcher.dispatch<ApplicationCloseEvent>([this](ApplicationCloseEvent& close)
        {
            ++close_events;
            close_exit_code = close.exit_code();
            return false;
        });
    }

    int32_t updates = 0;
    int32_t close_events = 0;
    int32_t close_exit_code = 0;
};

class SwallowingApp : public Application
{
public:
    SwallowingApp()
        : Application({})
    {
        counter = &push_layer<CloseCountingLayer>();
    }

    CloseCountingLayer* counter = nullptr;

protected:
    void on_event(Event& event) override
    {
        Application::on_event(event);
        event.handled = true;
    }
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

TEST_CASE("close() posts one ApplicationCloseEvent and the loop ends after the current frame")
{
    Application app({});
    CloseCountingLayer& layer = app.push_layer<CloseCountingLayer>();
    CHECK_FALSE(app.closing());

    app.run();

    CHECK(app.closing());
    CHECK(layer.close_events == 1);
    CHECK(layer.close_exit_code == 2);
    CHECK(layer.updates == 2);
    CHECK(app.exit_code() == 2);
}

TEST_CASE("A WindowCloseEvent closes the application even when a derived on_event swallows it")
{
    SwallowingApp app;
    WindowCloseEvent event;
    app.post_event(event);

    CHECK(app.closing());
    CHECK(app.counter->close_events == 0);
}

TEST_CASE("A WindowCloseEvent reaches the layers and closes the application")
{
    Application app({});
    CloseCountingLayer& layer = app.push_layer<CloseCountingLayer>();
    WindowCloseEvent event;
    app.post_event(event);

    CHECK(app.closing());
    CHECK(layer.close_events == 1);
    CHECK(app.exit_code() == 0);
}
