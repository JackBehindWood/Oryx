#include "doctest.h"

#include "Oryx.h"

namespace {

class NullEvent : public oryx::Event
{
public:
    OX_EVENT_CLASS_TYPE(None)
    OX_EVENT_CLASS_CATEGORY(oryx::EventCategoryApplication)
};

} // namespace

TEST_CASE("OX_EVENT_CLASS_TYPE/CATEGORY wire up event_type/name/category_flags")
{
    NullEvent e;

    CHECK(e.event_type() == oryx::EventType::None);
    CHECK(std::string(e.name()) == "None");
    CHECK(e.to_string() == "None");
    CHECK(e.is_in_category(oryx::EventCategoryApplication));
    CHECK_FALSE(e.handled);
}

TEST_CASE("EventDispatcher dispatches to a matching type and ORs the handler result into handled")
{
    NullEvent e;
    oryx::EventDispatcher dispatcher(e);

    bool called = false;
    bool dispatched = dispatcher.dispatch<NullEvent>([&](NullEvent&) {
        called = true;
        return true;
    });

    CHECK(dispatched);
    CHECK(called);
    CHECK(e.handled);
}

TEST_CASE("EventDispatcher leaves handled false when the handler declines the event")
{
    NullEvent e;
    oryx::EventDispatcher dispatcher(e);

    dispatcher.dispatch<NullEvent>([](NullEvent&) { return false; });

    CHECK_FALSE(e.handled);
}

TEST_CASE("AppTickEvent is a distinct EventType from a differently-typed event")
{
    oryx::AppTickEvent tick;
    NullEvent none;

    CHECK(tick.event_type() == oryx::EventType::AppTick);
    CHECK(none.event_type() == oryx::EventType::None);
    CHECK(tick.event_type() != none.event_type());
}

TEST_CASE("EventDispatcher only invokes the handler for a matching EventType")
{
    oryx::AppTickEvent tick;
    oryx::EventDispatcher dispatcher(tick);

    bool null_handler_called = false;
    bool dispatched_as_none = dispatcher.dispatch<NullEvent>([&](NullEvent&) {
        null_handler_called = true;
        return true;
    });

    CHECK_FALSE(dispatched_as_none);
    CHECK_FALSE(null_handler_called);
    CHECK_FALSE(tick.handled);

    bool tick_handler_called = false;
    bool dispatched_as_tick = dispatcher.dispatch<oryx::AppTickEvent>([&](oryx::AppTickEvent&) {
        tick_handler_called = true;
        return true;
    });

    CHECK(dispatched_as_tick);
    CHECK(tick_handler_called);
    CHECK(tick.handled);
}
