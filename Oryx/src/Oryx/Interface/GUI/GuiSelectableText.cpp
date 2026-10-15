#include "oxpch.h"
#include "Oryx/Interface/GUI/GuiSelectableText.h"
#include "Oryx/Interface/GUI/GuiTextEdit.h"

#include "Oryx/Interface/GUI/GuiTextField.h"

namespace oryx::gui
{

namespace
{

constexpr float k_pad = 2.0f;
constexpr float k_line_extra = 4.0f;
constexpr float k_highlight_alpha = 0.4f;

uint32_t line_count(std::string_view text)
{
    return static_cast<uint32_t>(std::count(text.begin(), text.end(), '\n')) + 1;
}

std::string_view line_at(std::string_view text, uint32_t index)
{
    size_t start = 0;
    for (uint32_t line = 0; line < index; ++line)
    {
        const size_t end = text.find('\n', start);
        if (end == std::string_view::npos)
        {
            return {};
        }
        start = end + 1;
    }
    const size_t end = text.find('\n', start);
    return text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
}

size_t offset_of(std::string_view text, uint32_t line, uint32_t byte)
{
    size_t start = 0;
    for (uint32_t index = 0; index < line; ++index)
    {
        const size_t end = text.find('\n', start);
        if (end == std::string_view::npos)
        {
            return text.size();
        }
        start = end + 1;
    }
    return start + byte;
}

bool is_before(uint32_t line_a, uint32_t byte_a, uint32_t line_b, uint32_t byte_b)
{
    return line_a < line_b || (line_a == line_b && byte_a < byte_b);
}

TextSelection clamped(TextSelection selection, std::string_view text)
{
    const uint32_t lines = line_count(text);
    selection.anchor_line = math::min(selection.anchor_line, lines - 1);
    selection.caret_line = math::min(selection.caret_line, lines - 1);
    selection.anchor_byte = math::min(selection.anchor_byte, static_cast<uint32_t>(line_at(text, selection.anchor_line).size()));
    selection.caret_byte = math::min(selection.caret_byte, static_cast<uint32_t>(line_at(text, selection.caret_line).size()));
    return selection;
}

TextSelection everything(std::string_view text)
{
    const uint32_t last = line_count(text) - 1;
    return { 0, 0, last, static_cast<uint32_t>(line_at(text, last).size()) };
}

// The text between the two ends in document order.
std::string_view range_of(const TextSelection& selection, std::string_view text)
{
    const bool forward = !is_before(selection.caret_line, selection.caret_byte, selection.anchor_line, selection.anchor_byte);
    const size_t from = forward ? offset_of(text, selection.anchor_line, selection.anchor_byte) : offset_of(text, selection.caret_line, selection.caret_byte);
    const size_t to = forward ? offset_of(text, selection.caret_line, selection.caret_byte) : offset_of(text, selection.anchor_line, selection.anchor_byte);
    return text.substr(from, to - from);
}

} // namespace

ItemState selectable_text(std::string_view id, std::string_view text, const SelectableTextOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = ctx.role_style(options, &GuiTheme::panel);
    const ImId block = ctx.id(id);
    const ImInput& input = ctx.input();
    const float line_height = style.text_height + k_line_extra;
    const uint32_t lines = line_count(text);

    const ItemState item = ctx.item(block);
    const ItemDrag drag = ctx.item_drag(block);
    TextSelection selection = clamped(ctx.state<TextSelection>(block), text);
    bool focused = ctx.focus() == block;
    Rect rect;
    const bool known = ctx.layout_rect(block, rect);

    const auto hit = [&](uint32_t& line, uint32_t& byte)
    {
        const float row = math::floor((input.pointer.position[1] - rect.min[1]) / line_height);
        line = static_cast<uint32_t>(math::clamp(row, 0.0f, static_cast<float>(lines - 1)));
        byte = detail::text_offset_at(ctx, line_at(text, line), style.text_height, input.pointer.position[0] - rect.min[0] - k_pad);
    };

    if (item.pressed)
    {
        ctx.set_focus(block);
        focused = true;
        if (item.double_clicked || !known)
        {
            selection = everything(text);
        }
        else
        {
            hit(selection.anchor_line, selection.anchor_byte);
            selection.caret_line = selection.anchor_line;
            selection.caret_byte = selection.anchor_byte;
        }
    }
    else if (item.triple_clicked)
    {
        selection = { selection.anchor_line, 0, selection.anchor_line, static_cast<uint32_t>(line_at(text, selection.anchor_line).size()) };
    }
    else if (item.double_clicked)
    {
        uint32_t low = 0;
        uint32_t high = 0;
        word_range(line_at(text, selection.anchor_line), selection.anchor_byte, low, high);
        selection = { selection.anchor_line, low, selection.anchor_line, high };
    }
    else if (focused && drag.dragging && known)
    {
        hit(selection.caret_line, selection.caret_byte);
    }
    else if (focused && (button_of(input, MouseCode::Left).pressed && !item.hovered || key_pressed(input.keys, ImKey::Escape)))
    {
        ctx.set_focus({});
        focused = false;
        selection.caret_line = selection.anchor_line;
        selection.caret_byte = selection.anchor_byte;
    }
    if (focused && input.keys.shortcut)
    {
        if (key_pressed(input.keys, ImKey::A))
        {
            selection = everything(text);
        }
        if (key_pressed(input.keys, ImKey::C) && !selection_empty(selection))
        {
            ctx.request_copy(range_of(selection, text));
        }
    }
    if (item.hovered || focused)
    {
        ctx.request_cursor(CursorShape::Text);
    }
    ctx.state<TextSelection>(block) = selection;

    const bool forward = !is_before(selection.caret_line, selection.caret_byte, selection.anchor_line, selection.anchor_byte);
    const uint32_t first_line = forward ? selection.anchor_line : selection.caret_line;
    const uint32_t first_byte = forward ? selection.anchor_byte : selection.caret_byte;
    const uint32_t last_line = forward ? selection.caret_line : selection.anchor_line;
    const uint32_t last_byte = forward ? selection.caret_byte : selection.anchor_byte;
    const bool highlight = focused && !selection_empty(selection);
    Colour tint = style.accent;
    tint.a = k_highlight_alpha;

    LayoutStyle column;
    column.width = options.width;
    column.direction = Direction::Column;
    ctx.begin_box(block, column);
    size_t start = 0;
    for (uint32_t line = 0; line < lines; ++line)
    {
        const size_t end = text.find('\n', start);
        const std::string_view content = text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
        start = end == std::string_view::npos ? text.size() : end + 1;

        LayoutStyle row;
        row.width = grow();
        row.height = fixed(line_height);
        row.align_y = Align::Centre;
        row.padding.left = k_pad;
        row.overflow = Overflow::Clip;
        ctx.begin_box(ImId{}, row);
        if (highlight && line >= first_line && line <= last_line)
        {
            const uint32_t from = line == first_line ? first_byte : 0;
            const uint32_t to = line == last_line ? last_byte : static_cast<uint32_t>(content.size());
            const float x0 = detail::measure_text(ctx, content.substr(0, from), style.text_height);
            const float x1 = detail::measure_text(ctx, content.substr(0, to), style.text_height) + (line < last_line ? style.text_height * 0.4f : 0.0f);
            LayoutStyle mark;
            mark.width = fixed(x1 - x0);
            mark.height = fixed(line_height);
            mark.floating = { true, AttachPoint::TopLeft, AttachPoint::TopLeft, FloatTarget::Parent, {}, { k_pad + x0, 0.0f } };
            const uint32_t mark_index = ctx.begin_box(ImId{}, mark);
            ctx.layout().node(mark_index).paint.has_fill = true;
            ctx.layout().node(mark_index).paint.fill = tint;
            ctx.end_box();
        }
        if (!content.empty())
        {
            LayoutStyle label;
            label.width = fit();
            label.height = fit();
            const uint32_t label_index = ctx.begin_box(ImId{}, label);
            BoxPaint& paint = ctx.layout().node(label_index).paint;
            paint.text = ctx.arena().store(content);
            paint.text_height = style.text_height;
            paint.text_colour = style.text;
            ctx.end_box();
        }
        ctx.end_box();
    }
    ctx.end_box();
    return item;
}

std::string_view selected_text(std::string_view id, std::string_view text)
{
    GuiContext& ctx = context();
    const TextSelection selection = clamped(ctx.state<TextSelection>(ctx.id(id)), text);
    return selection_empty(selection) ? std::string_view() : range_of(selection, text);
}

} // namespace oryx::gui
