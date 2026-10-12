#pragma once

#include "Oryx/Interface/GUI/GuiControls.h"

// Optional: one `edit(label, value)` for the common value types, each a thin dispatch to the control that edits it (same row, label, validation outline and ids as calling that control), so a settings or inspector panel reads as a list of fields.
// Nothing in the GUI includes this; the controls stay complete without it. Every overload returns whether the value changed.
namespace oryx::gui
{

inline bool edit(std::string_view label, bool& value, const FieldOptions& options = {})
{
    return checkbox(label, value, options);
}

inline bool edit(std::string_view label, int32_t& value, const NumberOptions& options = {})
{
    return input_int(label, value, options);
}

inline bool edit(std::string_view label, float& value, const NumberOptions& options = {})
{
    return input_float(label, value, options);
}

// Two number fields in one row, x then y, labelled `label`.
inline bool edit(std::string_view label, Vec2f& value, const NumberOptions& options = {})
{
    begin_field(label, options);
    bool changed = false;
    NumberOptions each = options;
    each.label_width = 0.0f;
    changed = input_float("##x", value[0], each);
    changed = input_float("##y", value[1], each) || changed;
    end_field();
    return changed;
}

// Edits the NUL-terminated text in a char array; Enter or losing focus is the caller's to read from the result.
template<size_t N>
inline TextInputResult edit(std::string_view label, char (&buffer)[N], const FieldOptions& options = {})
{
    return text_input(label, buffer, static_cast<uint32_t>(N), options);
}

// Picks one of `count` named values of an enum (or integer index) with a combo; names[i] is the text of value i.
template<typename Enum>
    requires std::is_enum_v<Enum>
inline bool edit(std::string_view label, Enum& value, const char* const* names, uint32_t count, const FieldOptions& options = {})
{
    const uint32_t current = static_cast<uint32_t>(value);
    bool changed = false;
    if (ComboScope combo(label, current < count ? names[current] : "", options); combo)
    {
        for (uint32_t index = 0; index < count; ++index)
        {
            if (combo.item(names[index], index == current))
            {
                value = static_cast<Enum>(index);
                changed = true;
            }
        }
    }
    return changed;
}

} // namespace oryx::gui
