#include "doctest.h"

#include "Oryx.h"

namespace {

class RecordingLayer : public oryx::Layer
{
public:
    RecordingLayer(std::string name, std::vector<std::string>& log)
        : oryx::Layer(std::move(name))
        , m_log(log)
    {
    }

    void attach() override { m_log.push_back(name() + ":attach"); }
    void detach() override { m_log.push_back(name() + ":detach"); }
    void update() override { m_log.push_back(name() + ":update"); }

private:
    std::vector<std::string>& m_log;
};

class NullEvent : public oryx::Event
{
public:
    OX_EVENT_CLASS_TYPE(None)
    OX_EVENT_CLASS_CATEGORY(oryx::EventCategoryApplication)
};

class HandlingLayer : public oryx::Layer
{
public:
    HandlingLayer(std::string name, std::vector<std::string>& log, bool should_handle)
        : oryx::Layer(std::move(name))
        , m_log(log)
        , m_should_handle(should_handle)
    {
    }

    void event(oryx::Event& e) override
    {
        m_log.push_back(name());
        if (m_should_handle)
        {
            e.handled = true;
        }
    }

private:
    std::vector<std::string>& m_log;
    bool m_should_handle;
};

} // namespace

TEST_CASE("LayerStack preserves push order and overlays land after layers")
{
    std::vector<std::string> log;
    oryx::LayerStack stack;

    stack.push_layer<RecordingLayer>("A", log);
    stack.push_overlay<RecordingLayer>("Overlay", log);
    stack.push_layer<RecordingLayer>("B", log);

    std::vector<std::string> order;
    for (auto& layer : stack)
    {
        order.push_back(layer->name());
    }

    CHECK(order == std::vector<std::string>{ "A", "B", "Overlay" });
}

TEST_CASE("LayerStack::push_layer constructs the layer, attaches it immediately, and returns a usable reference")
{
    std::vector<std::string> log;
    oryx::LayerStack stack;

    auto& a = stack.push_layer<RecordingLayer>("A", log);
    auto& b = stack.push_layer<RecordingLayer>("B", log);

    CHECK(log == std::vector<std::string>{ "A:attach", "B:attach" });
    CHECK(a.name() == "A");
    CHECK(b.name() == "B");

    log.clear();
    for (auto& layer : stack)
    {
        layer->update();
    }

    CHECK(log == std::vector<std::string>{ "A:update", "B:update" });
}

TEST_CASE("LayerStack detaches layers in reverse order on destruction")
{
    std::vector<std::string> log;
    {
        oryx::LayerStack stack;
        stack.push_layer<RecordingLayer>("A", log);
        stack.push_layer<RecordingLayer>("B", log);
        log.clear();
    }

    CHECK(log == std::vector<std::string>{ "B:detach", "A:detach" });
}

TEST_CASE("LayerStack event propagation is top-down and stops once handled")
{
    std::vector<std::string> log;
    oryx::LayerStack stack;

    stack.push_layer<HandlingLayer>("A", log, false);
    stack.push_layer<HandlingLayer>("B", log, true);
    stack.push_layer<HandlingLayer>("C", log, false);

    NullEvent e;
    for (auto it = stack.rbegin(); it != stack.rend(); ++it)
    {
        if (e.handled)
        {
            break;
        }
        (*it)->event(e);
    }

    CHECK(log == std::vector<std::string>{ "C", "B" });
    CHECK(e.handled);
}

namespace
{

enum class Phase
{
    Attach,
    Update,
    Event,
    Detach
};

class FailingLayer : public oryx::Layer
{
public:
    FailingLayer(std::string name, std::vector<std::string>& log, Phase failing_phase, bool throw_oryx_error = true)
        : oryx::Layer(std::move(name))
        , m_log(log)
        , m_failing_phase(failing_phase)
        , m_throw_oryx_error(throw_oryx_error)
    {
    }

    void attach() override { run(Phase::Attach, "attach"); }
    void detach() override { run(Phase::Detach, "detach"); }
    void update() override { run(Phase::Update, "update"); }
    void event(oryx::Event&) override { run(Phase::Event, "event"); }

private:
    void run(Phase phase, const char* label)
    {
        m_log.push_back(name() + ":" + label);
        if (phase != m_failing_phase)
        {
            return;
        }
        if (m_throw_oryx_error)
        {
            throw oryx::Error("layer failure", "detail text");
        }
        throw std::runtime_error("plain failure");
    }

    std::vector<std::string>& m_log;
    Phase m_failing_phase;
    bool m_throw_oryx_error;
};

} // namespace

TEST_CASE("A layer that throws in update is disabled and skipped afterwards while other layers keep running")
{
    std::vector<std::string> log;
    oryx::LayerStack stack;
    stack.push_layer<RecordingLayer>("A", log);
    FailingLayer& failing = stack.push_layer<FailingLayer>("F", log, Phase::Update);
    stack.push_layer<RecordingLayer>("B", log);
    log.clear();

    CHECK_NOTHROW(stack.update());
    CHECK(failing.is_disabled());
    CHECK(log == std::vector<std::string>{ "A:update", "F:update", "B:update" });

    log.clear();
    stack.update();
    CHECK(log == std::vector<std::string>{ "A:update", "B:update" });
}

TEST_CASE("A layer that throws a plain std::exception is handled the same way")
{
    std::vector<std::string> log;
    oryx::LayerStack stack;
    FailingLayer& failing = stack.push_layer<FailingLayer>("F", log, Phase::Update, /*throw_oryx_error=*/false);

    CHECK_NOTHROW(stack.update());
    CHECK(failing.is_disabled());
}

TEST_CASE("A layer that throws in attach is disabled but stays in the stack and is still detached")
{
    std::vector<std::string> log;
    {
        oryx::LayerStack stack;
        FailingLayer& failing = stack.push_layer<FailingLayer>("F", log, Phase::Attach);
        CHECK(failing.is_disabled());

        log.clear();
        stack.update();
        CHECK(log.empty());
    }

    CHECK(log == std::vector<std::string>{ "F:detach" });
}

TEST_CASE("A layer that throws while handling an event is disabled and propagation continues below it")
{
    std::vector<std::string> log;
    oryx::LayerStack stack;
    stack.push_layer<HandlingLayer>("A", log, false);
    FailingLayer& failing = stack.push_layer<FailingLayer>("F", log, Phase::Event);
    stack.push_layer<HandlingLayer>("C", log, false);
    log.clear();

    NullEvent e;
    CHECK_NOTHROW(stack.dispatch_event(e));
    CHECK(failing.is_disabled());
    CHECK(log == std::vector<std::string>{ "C", "F:event", "A" });

    log.clear();
    NullEvent again;
    stack.dispatch_event(again);
    CHECK(log == std::vector<std::string>{ "C", "A" });
}

TEST_CASE("LayerStack::dispatch_event stops once a layer handles the event")
{
    std::vector<std::string> log;
    oryx::LayerStack stack;
    stack.push_layer<HandlingLayer>("A", log, false);
    stack.push_layer<HandlingLayer>("B", log, true);
    stack.push_layer<HandlingLayer>("C", log, false);

    NullEvent e;
    stack.dispatch_event(e);

    CHECK(log == std::vector<std::string>{ "C", "B" });
    CHECK(e.handled);
}

TEST_CASE("A layer that throws in detach does not stop the remaining layers from detaching")
{
    std::vector<std::string> log;
    {
        oryx::LayerStack stack;
        stack.push_layer<RecordingLayer>("A", log);
        stack.push_layer<FailingLayer>("F", log, Phase::Detach);
        stack.push_layer<RecordingLayer>("B", log);
        log.clear();
    }

    CHECK(log == std::vector<std::string>{ "B:detach", "F:detach", "A:detach" });
}
