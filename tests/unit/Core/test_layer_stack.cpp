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
