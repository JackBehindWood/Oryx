#pragma once

#include "Oryx/Interface/GUI/GuiTextEdit.h"
#include "Oryx/Interface/GUI/GuiWidgets.h"

// Private to Interface/GUI: the editable box that text_input, input_float/int and the slider and drag entry share.
namespace oryx::gui::detail
{

inline constexpr uint32_t k_number_capacity = 40;

// What a numeric field keeps between frames under its id: the text being edited, its editor state and whether an edit is under way.
struct NumberEntry
{
    TextEditState edit;
    char text[k_number_capacity] = {};
    bool editing = false;
};

static_assert(std::is_trivially_copyable_v<NumberEntry> && sizeof(NumberEntry) <= k_max_widget_state_size);

struct EditOutcome
{
    ItemState item;
    TextEditResult result;
    // The field held keyboard focus when the frame ended.
    bool focused = false;
};

// Draws the field named by `id` over `buffer` and runs the editor on this frame's input while it has focus. A press focuses it and places the caret; a double click selects everything; a press elsewhere, Enter and Escape release focus.
EditOutcome edit_field(GuiContext& ctx, ImId id, const ImStyle& style, char* buffer, uint32_t capacity, TextEditState& state, Sizing width);

// The whole string as a number, spaces around it allowed; false when anything else is in it.
[[nodiscard]] bool parse_number(std::string_view text, double& out);

// `value` as text: whole numbers for integers, else `decimals` places with trailing zeros dropped.
void format_number(char* out, uint32_t capacity, double value, bool integer, uint32_t decimals);

// Starts an edit of `value` in `entry`: formatted, fully selected, and focused on `id`.
void begin_number_entry(GuiContext& ctx, ImId id, NumberEntry& entry, double value, bool integer, uint32_t decimals);

// While `entry` is editing, draws the field in place of the control and returns true once the edit ended; `value` is then the parsed number (unchanged on a bad parse or Escape) and `committed` says it was taken.
bool run_number_entry(GuiContext& ctx, ImId id, const ImStyle& style, NumberEntry& entry, double& value, bool& committed);

} // namespace oryx::gui::detail
