#include "BoardOverlay2D.h"

namespace oryx
{

namespace
{

constexpr float k_option_button_width = 120.0f;
constexpr float k_option_button_height = 36.0f;
constexpr float k_option_button_gap = 12.0f;
constexpr float k_banner_padding = 28.0f;
constexpr float k_banner_gap = 14.0f;
constexpr float k_again_width = 180.0f;
constexpr float k_again_height = 40.0f;
constexpr float k_chip_width = 76.0f;
constexpr float k_chip_height = 28.0f;
constexpr float k_chip_gap = 6.0f;
constexpr float k_corner_margin = 12.0f;
constexpr float k_legend_gap = 8.0f;
constexpr float k_badge_height = 20.0f;
constexpr float k_badge_text = 13.0f;
constexpr float k_swatch_width = 14.0f;
constexpr float k_swatch_height = 10.0f;
constexpr uint32_t k_swatch_count = 6;

void build_banner(const BoardOverlayText& text, BoardOverlayResult& out)
{
    LayoutStyle box = ui::anchored(AttachPoint::Centre, fit(), fit());
    box.direction = Direction::Column;
    box.align_x = Align::Centre;
    box.padding = uniform_insets(k_banner_padding);
    box.gap = k_banner_gap;
    ui::WidgetOptions banner_options;
    banner_options.variant = "banner";
    banner_options.layout = &box;
    ui::PanelScope banner("result", banner_options);

    LayoutStyle line;
    ui::WidgetOptions headline_options;
    headline_options.variant = text.headline_variant;
    headline_options.layout = &line;
    ui::label(text.headline, headline_options);
    ui::WidgetOptions hint_options;
    hint_options.variant = "hint";
    hint_options.layout = &line;
    ui::label(text.hint, hint_options);

    LayoutStyle again;
    again.width = fixed(k_again_width);
    again.height = fixed(k_again_height);
    again.align_x = Align::Centre;
    again.align_y = Align::Centre;
    ui::WidgetOptions again_options;
    again_options.variant = "primary";
    again_options.layout = &again;
    out.restart = ui::button("Play again", again_options).clicked;
}

void build_chips(const BoardAnnotationOverlay& annotations, BoardOverlayResult& out)
{
    LayoutStyle row = ui::anchored(AttachPoint::TopRight, fit(), fixed(k_chip_height), { -k_corner_margin, k_corner_margin });
    row.gap = k_chip_gap;
    ui::BoxScope chips("annotation-chips", row);

    LayoutStyle chip_box;
    chip_box.width = fixed(k_chip_width);
    chip_box.height = fixed(k_chip_height);
    ui::WidgetOptions options;
    options.layout = &chip_box;
    AnnotationFlags flags = annotations.flags;
    ui::toggle("Heat", flags.heat, options);
    ui::toggle("Chosen", flags.chosen, options);
    ui::toggle("Values", flags.value, options);
    out.annotation_flags = flags;
}

void build_badge(UiContext& ui, std::string_view name, std::string_view text, const Colour& fill, const Colour& text_colour)
{
    LayoutStyle box;
    box.width = fit();
    box.height = fixed(k_badge_height);
    box.padding = { 8.0f, 0.0f, 8.0f, 0.0f };
    box.align_y = Align::Centre;
    const uint32_t index = ui::begin_box(name, box);
    BoxPaint& paint = ui.layout().node(index).paint;
    paint.has_fill = true;
    paint.fill = fill;
    paint.radius = uniform_radius(k_badge_height * 0.5f);
    paint.text = text;
    paint.text_height = k_badge_text;
    paint.text_colour = text_colour;
    ui::end_box();
}

void build_legend(UiContext& ui, const BoardAnnotationOverlay& annotations)
{
    LayoutStyle row = ui::anchored(AttachPoint::BottomLeft, fit(), fixed(k_badge_height), { k_corner_margin, -k_corner_margin });
    row.gap = k_legend_gap;
    row.align_y = Align::Centre;
    ui::BoxScope legend("annotation-legend", row);

    if (annotations.ghost)
    {
        build_badge(ui, "ghost", "Ghost", { 0.98f, 0.82f, 0.25f, 1.0f }, { 0.07f, 0.08f, 0.11f, 1.0f });
    }
    if (!annotations.legend.empty())
    {
        LayoutStyle line;
        ui::WidgetOptions options;
        options.variant = "hint";
        options.layout = &line;
        ui::label(annotations.legend, options);
    }
    if (!annotations.heat_scale)
    {
        return;
    }
    LayoutStyle swatches;
    swatches.align_y = Align::Centre;
    ui::BoxScope scale("heat-scale", swatches);
    for (uint32_t step = 0; step < k_swatch_count; ++step)
    {
        LayoutStyle cell;
        cell.width = fixed(k_swatch_width);
        cell.height = fixed(k_swatch_height);
        const uint32_t index = ui.begin_box(ui.index_id(step), cell);
        BoxPaint& paint = ui.layout().node(index).paint;
        paint.has_fill = true;
        paint.fill = lerp(annotations.heat_low, annotations.heat_high, static_cast<float>(step) / static_cast<float>(k_swatch_count - 1));
        ui.end_box();
    }
}

void build_overlay(UiContext& ui, const BoardScene& scene, const BoardOverlayText& text, BoardOverlayResult& out)
{
    LayoutStyle root;
    root.width = grow();
    root.height = grow();
    root.padding = { k_board_gutter, k_board_status_band, k_board_gutter, k_board_menu_band + k_board_gutter };
    ui::BoxScope overlay("overlay", root);

    {
        LayoutStyle board;
        board.width = grow();
        board.height = grow();
        ui::BoxScope region("board", board);
    }

    ui::StatusOptions status_options;
    status_options.at = AttachPoint::TopCentre;
    status_options.margin = (k_board_status_band - ui.ui_theme().status.text_height) * 0.5f;
    status_options.variant = text.status_variant;
    if (!text.status.empty())
    {
        ui::status_line(text.status, status_options);
    }
    if (!text.headline.empty())
    {
        build_banner(text, out);
    }
    if (text.annotations.controls)
    {
        build_chips(text.annotations, out);
        build_legend(ui, text.annotations);
    }

    if (scene.options.empty())
    {
        return;
    }
    LayoutStyle strip = ui::anchored(AttachPoint::BottomCentre, fit(), fixed(k_board_menu_band));
    strip.gap = k_option_button_gap;
    strip.align_y = Align::Centre;
    ui::BoxScope options("options", strip);

    LayoutStyle button_box;
    button_box.width = fixed(k_option_button_width);
    button_box.height = fixed(k_option_button_height);
    button_box.align_x = Align::Centre;
    button_box.align_y = Align::Centre;
    ui::WidgetOptions button_options;
    button_options.layout = &button_box;

    for (size_t index = 0; index < scene.options.size(); ++index)
    {
        const std::string& label = scene.options[index].label;
        ui.push_id(ui.index_id(index));
        const ImId id = ui.id(label);
        const bool clicked = ui::button(label, button_options).clicked;
        ui.pop_id();
        out.buttons.push_back({ id, {} });
        if (clicked && !out.chosen)
        {
            out.chosen = true;
            out.option = index;
        }
    }
}

} // namespace

ImInput board_input_to_im(const BoardInput& input, double delta_time, bool pointer_captured)
{
    ImInput result;
    result.surface_size = input.viewport;
    result.scale = input.scale;
    result.delta_time = static_cast<float>(delta_time);
    result.pointer.position = input.cursor;
    result.pointer.valid = !pointer_captured && input.cursor[0] >= 0.0f && input.cursor[1] >= 0.0f && input.cursor[0] < input.viewport[0] && input.cursor[1] < input.viewport[1];
    ImButton& left = result.pointer.buttons[static_cast<uint32_t>(MouseCode::Left)];
    left.pressed = input.select;
    left.down = input.select_down || input.select;
    left.released = input.select_released;
    return result;
}

void run_board_overlay(UiContext& ui, const BoardInput& input, double delta_time, bool pointer_captured, const BoardScene& scene, const BoardOverlayText& text, BoardOverlayResult& out)
{
    out.viewport = input.viewport;
    out.board = {};
    out.buttons.clear();
    out.option = 0;
    out.chosen = false;
    out.restart = false;
    out.annotation_flags = text.annotations.flags;
    out.pointer_over_ui = false;

    ContextScope<UiContext> scope(ui);
    ui.begin_frame(board_input_to_im(input, delta_time, pointer_captured));
    try
    {
        build_overlay(ui, scene, text, out);
        ui.end_frame();
    }
    catch (...)
    {
        ui.abort_frame();
        throw;
    }

    if (!ui.layout_rect(ui.id("board"), out.board))
    {
        out.board = {};
    }
    for (OverlayButton& button : out.buttons)
    {
        if (!ui.layout_rect(button.id, button.rect))
        {
            button.rect = {};
        }
    }
    out.pointer_over_ui = ui.wants_mouse();
}

} // namespace oryx
