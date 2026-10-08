#include "oxpch.h"
#include "Oryx/Interface/GUI/GuiWorkspace.h"

namespace oryx::gui
{

void begin_workspace(std::string_view name)
{
    GuiContext& ctx = context();
    const Vec2f surface = ctx.input().surface_size;
    LayoutStyle root;
    root.width = fixed(surface[0]);
    root.height = fixed(surface[1]);
    root.direction = Direction::Column;
    ctx.begin_box(name, root);
    ctx.push_id(name);
}

void end_workspace()
{
    GuiContext& ctx = context();
    ctx.pop_id();
    ctx.end_box();
}

void begin_body()
{
    GuiContext& ctx = context();
    LayoutStyle row;
    row.width = grow();
    row.height = grow();
    row.direction = Direction::Row;
    ctx.begin_box("body", row);
    ctx.push_id("body");
}

void end_body()
{
    GuiContext& ctx = context();
    ctx.pop_id();
    ctx.end_box();
}

void central_area()
{
    GuiContext& ctx = context();
    LayoutStyle box;
    box.width = grow();
    box.height = grow();
    ctx.note_workspace_area(WorkspaceArea::Central, ctx.id("central"));
    ctx.begin_box("central", box);
    ctx.end_box();
}

void begin_side_panel(Side side, float width, const WidgetOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = ctx.role_style(options, &GuiTheme::panel);
    const std::string_view name = side == Side::Left ? "left_panel" : "right_panel";
    LayoutStyle box = im::widget_box(style, options);
    if (options.layout == nullptr)
    {
        box.width = fixed(math::clamp(width, 0.0f, ctx.input().surface_size[0] * 0.5f));
        box.height = grow();
        box.direction = Direction::Column;
        box.align_x = Align::Start;
        box.align_y = Align::Start;
    }
    box.overflow = Overflow::Clip;
    ctx.note_workspace_area(side == Side::Left ? WorkspaceArea::Left : WorkspaceArea::Right, ctx.id(name));
    const uint32_t index = ctx.begin_box(name, box);
    im::paint_surface(ctx.layout().node(index).paint, style, style.background);
    ctx.push_id(name);
    begin_scroll("content");
}

void end_side_panel()
{
    GuiContext& ctx = context();
    end_scroll();
    ctx.pop_id();
    ctx.end_box();
}

} // namespace oryx::gui
