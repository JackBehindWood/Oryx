#pragma once

#include "Oryx.h"

namespace oasis
{

oryx::Colour playground_clear_colour(double time, oryx::Vec2f cursor, oryx::Vec2f size);

// Long-lived graphics sandbox: grows from a clear colour to Renderer demos without being renamed.
class PlaygroundLayer : public oryx::Layer
{
public:
    PlaygroundLayer();

    void attach() override;
    void update(double delta_time) override;
    void event(oryx::Event& event) override;

    [[nodiscard]] const oryx::Colour& clear_colour() const { return m_clear; }

private:
    bool on_window_resize(oryx::WindowResizeEvent& event);
    bool on_window_focus(oryx::WindowFocusEvent& event);
    bool on_window_close(oryx::WindowCloseEvent& event);
    bool on_key_pressed(oryx::KeyPressedEvent& event);
    bool on_key_released(oryx::KeyReleasedEvent& event);
    bool on_mouse_moved(oryx::MouseMovedEvent& event);
    bool on_mouse_button_pressed(oryx::MouseButtonPressedEvent& event);
    bool on_mouse_button_released(oryx::MouseButtonReleasedEvent& event);
    bool on_mouse_scrolled(oryx::MouseScrolledEvent& event);

    oryx::Window* m_window = nullptr;
    oryx::Colour m_clear;
    oryx::Vec2f m_size;
    double m_time = 0.0;
};

} // namespace oasis
