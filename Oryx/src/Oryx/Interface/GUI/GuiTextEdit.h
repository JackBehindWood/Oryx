#pragma once

#include "Oryx/Interface/Canvas/ImInput.h"

namespace oryx
{

// Where the caret and the other end of the selection sit, as byte offsets into the buffer (always on a code point boundary). Plain data a widget keeps in its per-id state.
struct TextEditState
{
    uint32_t caret = 0;
    uint32_t anchor = 0;
    // Horizontal scroll of the text inside its field, in points; the widget owns it.
    float scroll = 0.0f;
    // Seconds since the last edit or caret move, which drives the caret blink.
    float quiet = 0.0f;
};

static_assert(std::is_trivially_copyable_v<TextEditState> && std::is_standard_layout_v<TextEditState>);

// One frame of what the editor reacts to.
struct TextEditInput
{
    std::string_view typed;
    std::string_view paste;
    ImKeys keys;
    // Byte offset a click landed on, or k_no_click.
    uint32_t click = 0xFFFF'FFFFu;
    bool select_all = false;
};

static_assert(std::is_trivially_copyable_v<TextEditInput>);

inline constexpr uint32_t k_no_click = 0xFFFF'FFFFu;

struct TextEditResult
{
    // The buffer's text changed.
    bool changed = false;
    // Enter went down.
    bool submitted = false;
    // Escape went down.
    bool cancelled = false;
    // The selection should go to the clipboard (copy or cut; a cut has already removed it).
    bool copy = false;
    // The caret or selection moved.
    bool moved = false;
};

static_assert(std::is_trivially_copyable_v<TextEditResult> && std::is_standard_layout_v<TextEditResult>);

// Edits the NUL-terminated text in `buffer` (`capacity` bytes including the NUL): typing, backspace and delete, arrows with shift to select, home and end, shortcut+A/C/X/V, Enter and Escape, a click that places the caret.
// Text that does not fit is cut on a code point boundary; control characters and everything after a newline in pasted text are dropped. Pure: no allocation, nothing global.
// A copy or cut leaves the selection to read with `selection_text` taken before the call.
TextEditResult text_edit_apply(char* buffer, uint32_t capacity, TextEditState& state, const TextEditInput& input);
// The selected bytes (empty when the caret and anchor agree).
[[nodiscard]] std::string_view selection_text(const char* buffer, const TextEditState& state);
// Puts the caret and anchor at the end of the text and clamps them into the buffer; call when the text was replaced from outside.
void text_edit_reset(const char* buffer, TextEditState& state, bool select_everything);

} // namespace oryx
