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

TEST_CASE("release, scroll and focus events carry type, category and payload")
{
    oryx::MouseButtonReleasedEvent release(oryx::MouseCode::Right);
    CHECK(release.event_type() == oryx::EventType::MouseButtonReleased);
    CHECK(release.is_in_category(oryx::EventCategoryMouse));
    CHECK(release.button() == oryx::MouseCode::Right);

    oryx::MouseScrolledEvent scroll(0.5f, -2.0f);
    CHECK(scroll.event_type() == oryx::EventType::MouseScrolled);
    CHECK(scroll.is_in_category(oryx::EventCategoryInput));
    CHECK(scroll.dx() == 0.5f);
    CHECK(scroll.dy() == -2.0f);

    oryx::WindowFocusEvent focus(true);
    CHECK(focus.event_type() == oryx::EventType::WindowFocus);
    CHECK(focus.is_in_category(oryx::EventCategoryWindow));
    CHECK(focus.focused());
}
