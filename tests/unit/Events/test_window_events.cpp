#include "doctest.h"

#include "Oryx.h"

TEST_CASE("window, key and mouse events carry type, category and payload")
{
    oryx::WindowResizeEvent resize(800, 600);
    CHECK(resize.event_type() == oryx::EventType::WindowResize);
    CHECK(resize.is_in_category(oryx::EventCategoryWindow));
    CHECK(resize.width() == 800);
    CHECK(resize.height() == 600);

    oryx::KeyPressedEvent key(oryx::KeyCode::A);
    CHECK(key.event_type() == oryx::EventType::KeyPressed);
    CHECK(key.is_in_category(oryx::EventCategoryKeyboard));
    CHECK(key.is_in_category(oryx::EventCategoryInput));
    CHECK_FALSE(key.is_in_category(oryx::EventCategoryMouse));

    oryx::MouseButtonPressedEvent click(oryx::MouseCode::Left);
    oryx::EventDispatcher dispatcher(click);
    bool seen = false;
    CHECK(dispatcher.dispatch<oryx::MouseButtonPressedEvent>([&](oryx::MouseButtonPressedEvent&) { seen = true; return true; }));
    CHECK(seen);
    CHECK(click.button() == oryx::MouseCode::Left);
    CHECK(key.key() == oryx::KeyCode::A);
    CHECK(click.handled);
}
