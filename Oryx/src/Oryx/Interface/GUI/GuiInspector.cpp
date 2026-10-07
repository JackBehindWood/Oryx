#include "oxpch.h"
#include "Oryx/Interface/GUI/GuiInspector.h"

#include "Oryx/Core/Log.h"

namespace oryx::gui
{

namespace
{

struct InspectorState
{
    bool bounds = false;
    bool dump = false;
};

void outline(GuiContext& ctx, const Rect& rect, const Colour& colour)
{
    DrawList& draw = ctx.draw_list();
    const uint32_t previous = draw.current_channel();
    draw.split_channels(math::max(draw.channel_count(), k_channel_tooltip + 1));
    draw.set_channel(k_channel_tooltip);
    draw.add_border(rect, CornerRadius{}, 1.0f, colour);
    draw.set_channel(previous);
}

} // namespace

void inspector(std::string_view name, const WidgetOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = ctx.role_style(options, &GuiTheme::overlay);
    const ImStats stats = ctx.stats();
    const ImId hovered = ctx.input().pointer.valid ? ctx.item_at(ctx.input().pointer.position) : ImId{};
    Rect rect;
    const bool has_rect = is_valid(hovered) && ctx.previous_rect(hovered, rect);

    ColumnScope column(name, { .width = grow() });
    key_value("frame", ctx.arena().format("%llu", static_cast<unsigned long long>(stats.frame)), options);
    key_value("items", ctx.arena().format("%u", stats.items), options);
    key_value("boxes", ctx.arena().format("%u", stats.boxes), options);
    key_value("commands", ctx.arena().format("%u", stats.commands), options);
    key_value("popups", ctx.arena().format("%u", stats.popups), options);
    key_value("remembered", ctx.arena().format("%u", stats.remembered_items), options);
    key_value("arena", ctx.arena().format("%zu / %zu", stats.arena_used, stats.arena_capacity), options);
    key_value("hovered", is_valid(hovered) ? ctx.arena().format("%016llx", static_cast<unsigned long long>(hovered.value)) : "none", options);
    key_value("rect", has_rect ? ctx.arena().format("%.0f %.0f %.0f %.0f", rect.min[0], rect.min[1], rect.size[0], rect.size[1]) : "none", options);

    InspectorState state = ctx.state<InspectorState>(ctx.id("flags"));
    const bool was_dumping = state.dump;
    std::ignore = checkbox("layout bounds", state.bounds);
    std::ignore = checkbox("dump layout", state.dump);
    ctx.state<InspectorState>(ctx.id("flags")) = state;
    if (state.bounds && has_rect)
    {
        outline(ctx, rect, style.accent);
    }
    if (state.dump && !was_dumping)
    {
        OX_CORE_INFO("GUI layout of frame {}:\n{}", stats.frame, dump_layout(ctx));
    }
}

} // namespace oryx::gui
