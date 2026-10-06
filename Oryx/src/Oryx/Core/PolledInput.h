#pragma once

#include "Oryx/Core/Input.h"

namespace oryx
{

// Pressed/released/scroll are edge state for one frame: backends call begin_frame() before polling the OS.
class PolledInput final : public IInput
{
public:
    static constexpr int32_t k_key_count = 512;
    static constexpr int32_t k_mouse_button_count = 8;

    void begin_frame();
    void set_key(KeyCode key, bool down);
    void set_mouse_button(MouseCode button, bool down);
    void set_cursor(float x, float y);
    void add_scroll(float dx, float dy);

    [[nodiscard]] bool key_down(KeyCode key) const override;
    [[nodiscard]] bool key_pressed(KeyCode key) const override;
    [[nodiscard]] bool key_released(KeyCode key) const override;
    [[nodiscard]] bool mouse_down(MouseCode button) const override;
    [[nodiscard]] bool mouse_pressed(MouseCode button) const override;
    [[nodiscard]] bool mouse_released(MouseCode button) const override;
    void cursor_position(Vec2f& out) const override;
    void scroll_delta(Vec2f& out) const override;

private:
    struct Edge
    {
        bool down = false;
        bool pressed = false;
        bool released = false;
    };

    static void apply(Edge& edge, bool down);

    std::array<Edge, k_key_count> m_keys{};
    std::array<Edge, k_mouse_button_count> m_buttons{};
    float m_cursor_x = 0.0f;
    float m_cursor_y = 0.0f;
    float m_scroll_x = 0.0f;
    float m_scroll_y = 0.0f;
};

} // namespace oryx
