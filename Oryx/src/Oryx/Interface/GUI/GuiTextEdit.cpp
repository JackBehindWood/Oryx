#include "oxpch.h"
#include "Oryx/Interface/GUI/GuiTextEdit.h"

namespace oryx
{

namespace
{

bool is_continuation(char c)
{
    return (static_cast<uint8_t>(c) & 0xC0u) == 0x80u;
}

uint32_t previous_boundary(const char* text, uint32_t at)
{
    if (at == 0)
    {
        return 0;
    }
    --at;
    while (at > 0 && is_continuation(text[at]))
    {
        --at;
    }
    return at;
}

uint32_t next_boundary(const char* text, uint32_t length, uint32_t at)
{
    if (at >= length)
    {
        return length;
    }
    ++at;
    while (at < length && is_continuation(text[at]))
    {
        ++at;
    }
    return at;
}

uint32_t length_of(const char* buffer, uint32_t capacity)
{
    uint32_t length = 0;
    while (length < capacity && buffer[length] != '\0')
    {
        ++length;
    }
    return length;
}

uint32_t clamp_to_boundary(const char* text, uint32_t length, uint32_t at)
{
    at = math::min(at, length);
    while (at > 0 && at < length && is_continuation(text[at]))
    {
        --at;
    }
    return at;
}

struct Editor
{
    char* buffer;
    uint32_t capacity;
    uint32_t length;
    TextEditState& state;
    TextEditResult& result;

    uint32_t low() const { return math::min(state.caret, state.anchor); }
    uint32_t high() const { return math::max(state.caret, state.anchor); }
    bool has_selection() const { return state.caret != state.anchor; }

    void place(uint32_t at, bool extend)
    {
        const uint32_t before_caret = state.caret;
        const uint32_t before_anchor = state.anchor;
        state.caret = at;
        state.anchor = extend ? state.anchor : at;
        result.moved = result.moved || before_caret != state.caret || before_anchor != state.anchor;
    }

    void erase(uint32_t from, uint32_t to)
    {
        if (to <= from)
        {
            return;
        }
        std::memmove(buffer + from, buffer + to, length - to + 1);
        length -= to - from;
        state.caret = from;
        state.anchor = from;
        result.changed = true;
        result.moved = true;
    }

    void erase_selection()
    {
        erase(low(), high());
    }

    void insert(std::string_view text)
    {
        erase_selection();
        const uint32_t room = capacity - 1 - length;
        uint32_t take = 0;
        uint32_t index = 0;
        while (index < text.size())
        {
            const uint8_t lead = static_cast<uint8_t>(text[index]);
            if (lead == '\n' || lead == '\r')
            {
                break;
            }
            uint32_t step = 1;
            while (index + step < text.size() && is_continuation(text[index + step]))
            {
                ++step;
            }
            if (lead < 0x20 || lead == 0x7F)
            {
                index += step;
                continue;
            }
            if (take + step > room)
            {
                break;
            }
            std::memmove(buffer + state.caret + take + step, buffer + state.caret + take, length - state.caret - take + 1);
            std::memcpy(buffer + state.caret + take, text.data() + index, step);
            length += step;
            take += step;
            index += step;
        }
        if (take > 0)
        {
            state.caret += take;
            state.anchor = state.caret;
            result.changed = true;
            result.moved = true;
        }
    }
};

} // namespace

TextEditResult text_edit_apply(char* buffer, uint32_t capacity, TextEditState& state, const TextEditInput& input)
{
    TextEditResult result;
    if (buffer == nullptr || capacity == 0)
    {
        return result;
    }
    buffer[capacity - 1] = '\0';
    const uint32_t length = length_of(buffer, capacity - 1);
    state.caret = clamp_to_boundary(buffer, length, state.caret);
    state.anchor = clamp_to_boundary(buffer, length, state.anchor);
    Editor editor{ buffer, capacity, length, state, result };
    const ImKeys& keys = input.keys;

    if (input.select_all)
    {
        editor.state.anchor = 0;
        editor.state.caret = editor.length;
        result.moved = true;
    }
    else if (input.click != k_no_click)
    {
        editor.place(clamp_to_boundary(buffer, editor.length, input.click), keys.shift);
    }

    if (keys.shortcut)
    {
        if (key_pressed(keys, ImKey::A))
        {
            editor.state.anchor = 0;
            editor.state.caret = editor.length;
            result.moved = true;
        }
        if ((key_pressed(keys, ImKey::C) || key_pressed(keys, ImKey::X)) && editor.has_selection())
        {
            result.copy = true;
            if (key_pressed(keys, ImKey::X))
            {
                editor.erase_selection();
            }
        }
        if (key_pressed(keys, ImKey::V) && !input.paste.empty())
        {
            editor.insert(input.paste);
        }
    }
    else
    {
        if (key_stroke(keys, ImKey::Left))
        {
            editor.place(editor.has_selection() && !keys.shift ? editor.low() : previous_boundary(buffer, editor.state.caret), keys.shift);
        }
        if (key_stroke(keys, ImKey::Right))
        {
            editor.place(editor.has_selection() && !keys.shift ? editor.high() : next_boundary(buffer, editor.length, editor.state.caret), keys.shift);
        }
        if (key_stroke(keys, ImKey::Home))
        {
            editor.place(0, keys.shift);
        }
        if (key_stroke(keys, ImKey::End))
        {
            editor.place(editor.length, keys.shift);
        }
        if (key_stroke(keys, ImKey::Backspace))
        {
            if (editor.has_selection())
            {
                editor.erase_selection();
            }
            else
            {
                editor.erase(previous_boundary(buffer, editor.state.caret), editor.state.caret);
            }
        }
        if (key_stroke(keys, ImKey::Delete))
        {
            if (editor.has_selection())
            {
                editor.erase_selection();
            }
            else
            {
                editor.erase(editor.state.caret, next_boundary(buffer, editor.length, editor.state.caret));
            }
        }
        if (!input.typed.empty())
        {
            editor.insert(input.typed);
        }
    }
    result.submitted = key_pressed(keys, ImKey::Enter);
    result.cancelled = key_pressed(keys, ImKey::Escape);
    return result;
}

std::string_view selection_text(const char* buffer, const TextEditState& state)
{
    const uint32_t low = math::min(state.caret, state.anchor);
    const uint32_t high = math::max(state.caret, state.anchor);
    return std::string_view(buffer + low, high - low);
}

void text_edit_reset(const char* buffer, TextEditState& state, bool select_everything)
{
    const uint32_t length = static_cast<uint32_t>(std::strlen(buffer));
    state.caret = length;
    state.anchor = select_everything ? 0 : length;
    state.scroll = 0.0f;
    state.quiet = 0.0f;
}

} // namespace oryx
