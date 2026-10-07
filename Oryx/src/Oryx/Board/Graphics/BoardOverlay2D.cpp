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

    if (!ui.layout_rect(make_im_id("board"), out.board))
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
