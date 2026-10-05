#pragma once

#include "Oryx.h"

namespace oasis
{

// Long-lived graphics sandbox: owns a texture and draws through Renderer::begin_scene/draw_*/end_scene; GraphicsLayer owns the frame.
class PlaygroundLayer : public oryx::Layer
{
public:
    PlaygroundLayer();
    ~PlaygroundLayer() override;

    void attach() override;
    void detach() override;
    void update(double delta_time) override;
    void event(oryx::Event& event) override;

private:
    bool on_window_resize(oryx::WindowResizeEvent& event);
    bool on_window_focus(oryx::WindowFocusEvent& event);
    bool on_window_close(oryx::WindowCloseEvent& event);
    bool on_application_close(oryx::ApplicationCloseEvent& event);
    bool on_key_pressed(oryx::KeyPressedEvent& event);
    bool on_key_released(oryx::KeyReleasedEvent& event);
    bool on_mouse_moved(oryx::MouseMovedEvent& event);
    bool on_mouse_button_pressed(oryx::MouseButtonPressedEvent& event);
    bool on_mouse_button_released(oryx::MouseButtonReleasedEvent& event);
    bool on_mouse_scrolled(oryx::MouseScrolledEvent& event);

    oryx::Window* m_window = nullptr;
    oryx::UniquePtr<oryx::Texture2D> m_checker;
    oryx::Vec2f m_size;
    double m_time = 0.0;
};

} // namespace oasis
