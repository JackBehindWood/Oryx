#include "oxpch.h"
#include "Oryx/Core/Input.h"

#include "Oryx/Core/Application.h"

namespace oryx
{

namespace
{

const IInput* primary()
{
    Window* window = Application::Get().window();
    return window ? &window->input() : nullptr;
}

template<typename Query>
bool ask(Query query)
{
    const IInput* input = primary();
    return input && query(*input);
}

template<typename Query>
void fill(Vec2f& out, Query query)
{
    const IInput* input = primary();
    if (input)
    {
        query(*input, out);
    }
    else
    {
        out = Vec2f(0.0f, 0.0f);
    }
}

} // namespace

bool Input::key_down(KeyCode key) { return ask([key](const IInput& in) { return in.key_down(key); }); }
bool Input::key_pressed(KeyCode key) { return ask([key](const IInput& in) { return in.key_pressed(key); }); }
bool Input::key_released(KeyCode key) { return ask([key](const IInput& in) { return in.key_released(key); }); }
bool Input::mouse_down(MouseCode button) { return ask([button](const IInput& in) { return in.mouse_down(button); }); }
bool Input::mouse_pressed(MouseCode button) { return ask([button](const IInput& in) { return in.mouse_pressed(button); }); }
bool Input::mouse_released(MouseCode button) { return ask([button](const IInput& in) { return in.mouse_released(button); }); }
void Input::cursor_position(Vec2f& out) { fill(out, [](const IInput& in, Vec2f& v) { in.cursor_position(v); }); }
void Input::scroll_delta(Vec2f& out) { fill(out, [](const IInput& in, Vec2f& v) { in.scroll_delta(v); }); }

} // namespace oryx
