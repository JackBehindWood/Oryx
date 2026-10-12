#pragma once

#include "Oryx/Interface/GUI/GuiControls.h"

// Read-only text the pointer can select, like a label in a browser: drag to highlight, Cmd/Ctrl+A selects everything, Cmd/Ctrl+C copies the selection, a double click selects all, and the cursor turns into an I-beam over it.
namespace oryx::gui
{

struct SelectableTextOptions : WidgetOptions
{
    Sizing width = grow();
};

static_assert(std::is_trivially_copyable_v<SelectableTextOptions>);

// A selection as a line index and a byte offset in that line, so the same data describes a block here and a window onto many lines elsewhere. Kept in widget state under the widget's id.
struct TextSelection
{
    uint32_t anchor_line = 0;
    uint32_t anchor_byte = 0;
    uint32_t caret_line = 0;
    uint32_t caret_byte = 0;
};

static_assert(std::is_trivially_copyable_v<TextSelection> && std::is_standard_layout_v<TextSelection>);

[[nodiscard]] inline bool selection_empty(const TextSelection& selection)
{
    return selection.anchor_line == selection.caret_line && selection.anchor_byte == selection.caret_byte;
}

// `text` is split into lines at '\n' and drawn one line per row; `id` names the block and keeps its selection. The selection shows only while the block has keyboard focus, which a press gives it and a press elsewhere or Escape takes away.
// Returns the pointer's relation to the block.
ItemState selectable_text(std::string_view id, std::string_view text, const SelectableTextOptions& options = {});

// The part of `text` the block `id` has selected, empty when none; a view into `text`.
[[nodiscard]] std::string_view selected_text(std::string_view id, std::string_view text);

} // namespace oryx::gui
