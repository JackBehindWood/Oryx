#include "oxpch.h"
#include "Oryx/Interface/GUI/GuiModal.h"

namespace oryx::gui
{

namespace
{

struct ModalState
{
    bool open = false;
};

// The state lives under the dialog's id, which is also what the popup layer and close_current_modal see.
ImId dialog_id(const GuiContext& ctx, std::string_view name)
{
    return make_im_id("dialog", ctx.id(name));
}

// False when the modal is closed or Escape just closed it (`escaped`); true leaves the backdrop, dialog, popup layers and id scope open for end_modal.
bool begin_modal_impl(std::string_view name, const WidgetOptions& options, bool& escaped)
{
    GuiContext& ctx = context();
    const ImId dialog = dialog_id(ctx, name);
    if (!ctx.state<ModalState>(dialog).open)
    {
        return false;
    }
    const ImStyle& style = ctx.role_style(options, &GuiTheme::overlay);
    const Vec2f surface = ctx.input().surface_size;
    ctx.push_id(name);

    LayoutStyle backdrop;
    backdrop.width = fixed(surface[0]);
    backdrop.height = fixed(surface[1]);
    backdrop.channel = k_channel_popup;
    backdrop.floating = { true, AttachPoint::TopLeft, AttachPoint::TopLeft, FloatTarget::Root, {}, { 0.0f, 0.0f } };
    const ImId backdrop_id = ctx.id("backdrop");
    std::ignore = ctx.begin_popup_layer(backdrop_id);
    BoxPaint& dim = ctx.layout().node(ctx.begin_box(backdrop_id, backdrop)).paint;
    dim.has_fill = true;
    dim.fill = ctx.gui_theme().modal_dim;

    const PopupResult result = ctx.begin_popup_layer(dialog);
    LayoutStyle box = im::widget_box(style, options);
    if (options.layout == nullptr)
    {
        box.width = fit(200.0f);
        box.direction = Direction::Column;
        box.gap = ctx.gui_theme().spacing;
        box.align_x = Align::Start;
        box.align_y = Align::Start;
    }
    box.channel = k_channel_popup;
    box.floating = { true, AttachPoint::Centre, AttachPoint::Centre, FloatTarget::Parent, {}, { 0.0f, 0.0f } };
    im::paint_surface(ctx.layout().node(ctx.begin_box(dialog, box)).paint, style, style.background);
    if (result.closed_by_escape)
    {
        ctx.end_box();
        ctx.end_popup_layer();
        ctx.end_box();
        ctx.end_popup_layer();
        ctx.pop_id();
        ctx.state<ModalState>(dialog).open = false;
        escaped = true;
        return false;
    }
    return true;
}

} // namespace

void open_modal(std::string_view name)
{
    GuiContext& ctx = context();
    ctx.state<ModalState>(dialog_id(ctx, name)).open = true;
}

void close_modal(std::string_view name)
{
    GuiContext& ctx = context();
    ctx.state<ModalState>(dialog_id(ctx, name)).open = false;
}

void close_current_modal()
{
    GuiContext& ctx = context();
    if (is_valid(ctx.popup_id()))
    {
        ctx.state<ModalState>(ctx.popup_id()).open = false;
    }
}

bool begin_modal(std::string_view name, const WidgetOptions& options)
{
    bool escaped = false;
    return begin_modal_impl(name, options, escaped);
}

void end_modal()
{
    GuiContext& ctx = context();
    ctx.end_box();
    ctx.end_popup_layer();
    ctx.end_box();
    ctx.end_popup_layer();
    ctx.pop_id();
}

ModalChoice confirm(std::string_view name, std::string_view message, std::string_view confirm_label, std::string_view cancel_label)
{
    bool escaped = false;
    if (!begin_modal_impl(name, {}, escaped))
    {
        return escaped ? ModalChoice::Cancelled : ModalChoice::None;
    }
    ModalChoice choice = ModalChoice::None;
    label(message);
    begin_row("buttons", RowOptions{ grow(), fit(), {}, context().gui_theme().spacing, Align::End });
    if (button(confirm_label).clicked)
    {
        choice = ModalChoice::Confirmed;
    }
    if (button(cancel_label).clicked)
    {
        choice = ModalChoice::Cancelled;
    }
    end_row();
    if (choice != ModalChoice::None)
    {
        close_current_modal();
    }
    end_modal();
    return choice;
}

} // namespace oryx::gui
