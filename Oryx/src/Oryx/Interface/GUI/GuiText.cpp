#include "oxpch.h"
#include "Oryx/Interface/GUI/GuiTextField.h"

namespace oryx::gui
{

namespace detail
{

namespace
{

constexpr float k_pad = 6.0f;
constexpr float k_caret_width = 1.5f;

float text_width(GuiContext& ctx, std::string_view text, float height)
{
    Font* font = ctx.theme().font;
    return font != nullptr && !text.empty() ? font->measure(text, height).width : 0.0f;
}

uint32_t next_boundary(std::string_view text, uint32_t at)
{
    ++at;
    while (at < text.size() && (static_cast<uint8_t>(text[at]) & 0xC0u) == 0x80u)
    {
        ++at;
    }
    return at;
}

// The byte offset of the boundary nearest to `x` points into the text.
uint32_t index_at(GuiContext& ctx, std::string_view text, float height, float x)
{
    uint32_t at = 0;
    float previous = 0.0f;
    while (at < text.size())
    {
        const uint32_t next = next_boundary(text, at);
        const float width = text_width(ctx, text.substr(0, next), height);
        if (x < (previous + width) * 0.5f)
        {
            return at;
        }
        previous = width;
        at = next;
    }
    return static_cast<uint32_t>(text.size());
}

Colour with_alpha(const Colour& colour, float alpha)
{
    return { colour.r, colour.g, colour.b, alpha };
}

} // namespace

EditOutcome edit_field(GuiContext& ctx, ImId id, const ImStyle& style, char* buffer, uint32_t capacity, TextEditState& state, Sizing width)
{
    EditOutcome outcome;
    outcome.item = ctx.item(id);
    const ImInput& input = ctx.input();
    const float height = style.text_height + 6.0f;
    Rect rect;
    const bool known = ctx.layout_rect(id, rect);
    bool focused = ctx.focus() == id;
    const bool left_pressed = button_of(input, MouseCode::Left).pressed;
    TextEditInput edit;
    if (outcome.item.pressed)
    {
        if (!focused)
        {
            ctx.set_focus(id);
            focused = true;
        }
        if (outcome.item.double_clicked || !known)
        {
            edit.select_all = outcome.item.double_clicked;
        }
        else
        {
            const std::string_view text(buffer);
            edit.click = index_at(ctx, text, style.text_height, input.pointer.position[0] - rect.min[0] - k_pad + state.scroll);
        }
    }
    else if (outcome.item.double_clicked)
    {
        edit.select_all = true;
    }
    else if (focused && left_pressed && !outcome.item.hovered)
    {
        ctx.set_focus({});
        focused = false;
    }
    if (outcome.item.hovered || focused)
    {
        ctx.request_cursor(CursorShape::Text);
    }
    if (focused)
    {
        edit.typed = input.text;
        edit.paste = input.paste;
        edit.keys = input.keys;
        if (input.keys.shortcut && (key_pressed(input.keys, ImKey::C) || key_pressed(input.keys, ImKey::X)) && state.caret != state.anchor)
        {
            ctx.request_copy(selection_text(buffer, state));
        }
        outcome.result = text_edit_apply(buffer, capacity, state, edit);
        const bool activity = outcome.result.changed || outcome.result.moved || edit.click != k_no_click || edit.select_all;
        state.quiet = activity ? 0.0f : state.quiet + ctx.delta_time();
        if (outcome.result.submitted || outcome.result.cancelled)
        {
            ctx.set_focus({});
            focused = false;
        }
    }
    outcome.focused = focused;

    const std::string_view text(buffer);
    const float caret_x = text_width(ctx, text.substr(0, state.caret), style.text_height);
    const float inner = known ? math::max(1.0f, rect.size[0] - 2.0f * k_pad) : 1.0f;
    if (focused && known)
    {
        state.scroll = math::clamp(state.scroll, caret_x - inner + k_caret_width, caret_x);
    }
    state.scroll = math::max(0.0f, state.scroll);
    if (!focused)
    {
        state.scroll = 0.0f;
    }

    LayoutStyle box;
    box.width = width;
    box.height = fixed(height);
    box.overflow = Overflow::Clip;
    const uint32_t index = ctx.begin_box(id, box);
    im::paint_surface(ctx.layout().node(index).paint, style, im::interaction_fill(style, outcome.item, style.background));
    if (focused)
    {
        ctx.layout().node(index).paint.border = style.accent;
    }

    const auto floating = [](float x, float y) { return Floating{ true, AttachPoint::TopLeft, AttachPoint::TopLeft, FloatTarget::Parent, {}, { x, y } }; };
    if (focused && state.caret != state.anchor)
    {
        const uint32_t low = math::min(state.caret, state.anchor);
        const uint32_t high = math::max(state.caret, state.anchor);
        const float x0 = text_width(ctx, text.substr(0, low), style.text_height);
        const float x1 = text_width(ctx, text.substr(0, high), style.text_height);
        LayoutStyle selection;
        selection.width = fixed(x1 - x0);
        selection.height = fixed(height - 4.0f);
        selection.floating = floating(k_pad + x0 - state.scroll, 2.0f);
        const uint32_t selection_index = ctx.begin_box(ImId{}, selection);
        ctx.layout().node(selection_index).paint.has_fill = true;
        ctx.layout().node(selection_index).paint.fill = with_alpha(style.accent, 0.4f);
        ctx.end_box();
    }
    LayoutStyle label;
    label.width = fixed(text_width(ctx, text, style.text_height) + 2.0f);
    label.height = fixed(height);
    label.align_y = Align::Centre;
    label.floating = floating(k_pad - state.scroll, 0.0f);
    const uint32_t label_index = ctx.begin_box(ImId{}, label);
    BoxPaint& paint = ctx.layout().node(label_index).paint;
    paint.text = ctx.arena().store(text);
    paint.text_height = style.text_height;
    paint.text_colour = style.text;
    ctx.end_box();
    if (focused && std::fmod(state.quiet, 1.0f) < 0.6f)
    {
        LayoutStyle caret;
        caret.width = fixed(k_caret_width);
        caret.height = fixed(style.text_height);
        caret.floating = floating(k_pad + caret_x - state.scroll, (height - style.text_height) * 0.5f);
        const uint32_t caret_index = ctx.begin_box(ImId{}, caret);
        ctx.layout().node(caret_index).paint.has_fill = true;
        ctx.layout().node(caret_index).paint.fill = style.text;
        ctx.end_box();
    }
    ctx.end_box();
    return outcome;
}

bool parse_number(std::string_view text, double& out)
{
    while (!text.empty() && text.front() == ' ')
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ')
    {
        text.remove_suffix(1);
    }
    if (text.empty())
    {
        return false;
    }
    if (text.front() == '+')
    {
        text.remove_prefix(1);
    }
    double parsed = 0.0;
    const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc() || result.ptr != text.data() + text.size() || !std::isfinite(parsed))
    {
        return false;
    }
    out = parsed;
    return true;
}

void format_number(char* out, uint32_t capacity, double value, bool integer, uint32_t decimals)
{
    if (integer)
    {
        std::snprintf(out, capacity, "%lld", static_cast<long long>(std::llround(value)));
        return;
    }
    char scratch[k_number_capacity];
    std::snprintf(scratch, sizeof(scratch), "%.*f", static_cast<int>(decimals), value);
    uint32_t length = static_cast<uint32_t>(std::strlen(scratch));
    if (std::strchr(scratch, '.') != nullptr)
    {
        while (length > 1 && scratch[length - 1] == '0')
        {
            scratch[--length] = '\0';
        }
        if (scratch[length - 1] == '.')
        {
            scratch[--length] = '\0';
        }
    }
    std::snprintf(out, capacity, "%s", scratch);
}

void begin_number_entry(GuiContext& ctx, ImId id, NumberEntry& entry, double value, bool integer, uint32_t decimals)
{
    format_number(entry.text, k_number_capacity, value, integer, decimals);
    entry.editing = true;
    text_edit_reset(entry.text, entry.edit, true);
    ctx.set_focus(id);
}

bool run_number_entry(GuiContext& ctx, ImId id, const ImStyle& style, NumberEntry& entry, double& value, bool& committed)
{
    committed = false;
    const EditOutcome outcome = edit_field(ctx, id, style, entry.text, k_number_capacity, entry.edit, grow(1.0f, 80.0f));
    if (outcome.focused)
    {
        return false;
    }
    entry.editing = false;
    double parsed = value;
    committed = !outcome.result.cancelled && parse_number(entry.text, parsed);
    if (committed)
    {
        value = parsed;
    }
    return true;
}

} // namespace detail

TextInputResult text_input(std::string_view label, char* buffer, uint32_t capacity, const FieldOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = ctx.role_style(options, &GuiTheme::field);
    FieldScope field(label, options);
    const ImId id = ctx.id("input");
    TextEditState edit = ctx.state<TextEditState>(id);
    const detail::EditOutcome outcome = detail::edit_field(ctx, id, style, buffer, capacity, edit, grow(1.0f, 80.0f));
    ctx.state<TextEditState>(id) = edit;
    TextInputResult result;
    result.changed = outcome.result.changed;
    result.submitted = outcome.result.submitted;
    result.cancelled = outcome.result.cancelled;
    result.focused = outcome.focused;
    return result;
}

namespace
{

bool number_field(std::string_view label, double& value, double min, double max, bool integer, uint32_t decimals, bool* edit, const FieldOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = ctx.role_style(options, &GuiTheme::field);
    FieldScope field(label, options);
    const ImId id = ctx.id("input");
    detail::NumberEntry entry = ctx.state<detail::NumberEntry>(id);
    bool changed = false;
    if (!entry.editing && edit != nullptr && *edit)
    {
        detail::begin_number_entry(ctx, id, entry, value, integer, decimals);
    }
    if (!entry.editing)
    {
        detail::format_number(entry.text, detail::k_number_capacity, value, integer, decimals);
        const detail::EditOutcome outcome = detail::edit_field(ctx, id, style, entry.text, detail::k_number_capacity, entry.edit, grow(1.0f, 80.0f));
        if (outcome.focused)
        {
            entry.editing = true;
            text_edit_reset(entry.text, entry.edit, true);
        }
    }
    else
    {
        bool committed = false;
        double edited = value;
        if (detail::run_number_entry(ctx, id, style, entry, edited, committed) && committed)
        {
            const double clamped = min < max ? math::clamp(edited, min, max) : edited;
            const double result = integer ? std::round(clamped) : clamped;
            changed = result != value;
            value = result;
        }
    }
    ctx.state<detail::NumberEntry>(id) = entry;
    if (edit != nullptr)
    {
        *edit = entry.editing;
    }
    return changed;
}

} // namespace

bool input_float(std::string_view label, float& value, const NumberOptions& options)
{
    double number = static_cast<double>(value);
    const bool changed = number_field(label, number, static_cast<double>(options.min), static_cast<double>(options.max), false, options.decimals, options.edit, options);
    value = static_cast<float>(number);
    return changed;
}

bool input_int(std::string_view label, int32_t& value, const NumberOptions& options)
{
    double number = static_cast<double>(value);
    const bool changed = number_field(label, number, static_cast<double>(options.min), static_cast<double>(options.max), true, 0, options.edit, options);
    value = static_cast<int32_t>(number);
    return changed;
}

} // namespace oryx::gui
