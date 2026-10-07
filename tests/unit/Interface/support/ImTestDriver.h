#pragma once

#include "Oryx.h"

namespace oryx::test
{

// Scripted pointer, wheel and key input for any ImContext, one frame at a time. Edges (pressed, released, wheel, key presses) last one frame; levels (position, down) persist.
template<typename Context>
class ImTestDriver
{
public:
    explicit ImTestDriver(Context& context, const Vec2f& surface_size = { 800.0f, 600.0f }, float delta_time = 1.0f / 60.0f)
        : m_context(context)
    {
        m_input.surface_size = surface_size;
        m_input.delta_time = delta_time;
    }

    [[nodiscard]] Context& context() { return m_context; }
    [[nodiscard]] ImInput& input() { return m_input; }

    void move_to(const Vec2f& position)
    {
        m_input.pointer.valid = true;
        m_input.pointer.position = position;
    }
    void leave() { m_input.pointer.valid = false; }

    void press(MouseCode button = MouseCode::Left)
    {
        ImButton& state = m_input.pointer.buttons[static_cast<uint32_t>(button)];
        state.down = true;
        state.pressed = true;
    }
    void release(MouseCode button = MouseCode::Left)
    {
        ImButton& state = m_input.pointer.buttons[static_cast<uint32_t>(button)];
        state.down = false;
        state.released = true;
    }

    void wheel(const Vec2f& lines) { m_input.wheel = m_input.wheel + lines; }

    void key_press(ImKey key)
    {
        m_input.keys.down |= im_key_bit(key);
        m_input.keys.pressed |= im_key_bit(key);
    }
    void key_release(ImKey key) { m_input.keys.down &= ~im_key_bit(key); }
    // An OS auto-repeat of a held key for one frame.
    void key_repeat(ImKey key) { m_input.keys.repeated |= im_key_bit(key); }
    // Typed characters (UTF-8) for one frame.
    void type(std::string_view utf8)
    {
        m_text.append(utf8);
        m_input.text = m_text;
    }
    // The shortcut modifier held down (persists until released).
    void hold_shortcut(bool held)
    {
        m_shortcut_held = held;
        m_input.keys.shortcut = held;
    }
    // A shortcut + `key` stroke in one frame.
    void chord(ImKey key)
    {
        m_input.keys.shortcut = true;
        key_press(key);
    }
    // The clipboard text a paste delivers this frame.
    void paste(std::string_view text)
    {
        m_paste.assign(text);
        m_input.paste = m_paste;
    }

    // Runs one frame with the queued input, then drops the one-frame edges.
    template<typename Build>
    void frame(Build&& build)
    {
        m_context.begin_frame(m_input);
        build();
        m_context.end_frame();
        clear_edges();
    }

    template<typename Build>
    void run_frames(uint32_t count, Build&& build)
    {
        for (uint32_t index = 0; index < count; ++index)
        {
            frame(build);
        }
    }

    // Frames until the box tree stops changing (widgets answer from last frame's rects); returns the frames run, at most `limit`.
    template<typename Build>
    uint32_t settle(Build&& build, uint32_t limit = 8)
    {
        std::string previous;
        for (uint32_t count = 1; count <= limit; ++count)
        {
            frame(build);
            std::string current = dump_layout(m_context);
            if (current == previous)
            {
                return count;
            }
            previous = std::move(current);
        }
        return limit;
    }

    template<typename Build>
    void click(const Vec2f& at, Build&& build, MouseCode button = MouseCode::Left)
    {
        move_to(at);
        frame(build);
        press(button);
        frame(build);
        release(button);
        frame(build);
    }

    template<typename Build>
    void double_click(const Vec2f& at, Build&& build, MouseCode button = MouseCode::Left)
    {
        click(at, build, button);
        click(at, build, button);
    }

    template<typename Build>
    void drag(const Vec2f& from, const Vec2f& to, uint32_t steps, Build&& build, MouseCode button = MouseCode::Left)
    {
        move_to(from);
        frame(build);
        press(button);
        frame(build);
        for (uint32_t step = 1; step <= steps; ++step)
        {
            move_to(from + (to - from) * (static_cast<float>(step) / static_cast<float>(steps)));
            frame(build);
        }
        release(button);
        frame(build);
    }

private:
    void clear_edges()
    {
        for (ImButton& button : m_input.pointer.buttons)
        {
            button.pressed = false;
            button.released = false;
        }
        m_input.wheel = { 0.0f, 0.0f };
        m_input.keys.pressed = 0;
        m_input.keys.repeated = 0;
        m_input.keys.shortcut = m_shortcut_held;
        m_text.clear();
        m_input.text = {};
        m_paste.clear();
        m_input.paste = {};
    }

    Context& m_context;
    ImInput m_input;
    std::string m_text;
    std::string m_paste;
    bool m_shortcut_held = false;
};

} // namespace oryx::test
