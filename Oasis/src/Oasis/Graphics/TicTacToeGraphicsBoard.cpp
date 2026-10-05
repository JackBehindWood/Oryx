#include "TicTacToeGraphicsBoard.h"

namespace oasis
{

namespace
{

constexpr float kGridThickness = 4.0f;
constexpr float kMarkInset = 0.22f;
constexpr float kMarkThickness = 10.0f;
constexpr oryx::Colour kGridColour = { 0.85f, 0.85f, 0.9f, 1.0f };
constexpr oryx::Colour kXColour = { 0.95f, 0.45f, 0.35f, 1.0f };
constexpr oryx::Colour kOColour = { 0.35f, 0.7f, 0.95f, 1.0f };
constexpr oryx::Colour kHoverColour = { 1.0f, 1.0f, 1.0f, 0.08f };
constexpr oryx::Colour kStatusColour = { 1.0f, 1.0f, 1.0f, 1.0f };

const oryx::Colour& mark_colour(Mark mark)
{
    return mark == Mark::X ? kXColour : kOColour;
}

} // namespace

TicTacToeLayout TicTacToeGraphicsBoard::layout() const
{
    oryx::NativeWindowHandle handle = oryx::Application::Get().window()->native_handle();
    return TicTacToeLayout::fit({ static_cast<float>(handle.width), static_cast<float>(handle.height) });
}

oryx::Vec2f TicTacToeGraphicsBoard::cursor_world(const TicTacToeLayout& layout) const
{
    oryx::Vec2f cursor;
    oryx::Input::cursor_position(cursor);
    return layout.world_from_cursor(cursor);
}

void TicTacToeGraphicsBoard::on_turn(const oryx::IState& state)
{
    m_model.update(state);
}

oryx::ActionId TicTacToeGraphicsBoard::poll_action(const oryx::IState&)
{
    if (!oryx::Input::mouse_pressed(oryx::MouseCode::Left))
    {
        return oryx::PENDING_ACTION;
    }

    TicTacToeLayout current = layout();
    size_t row = 0;
    size_t col = 0;
    if (!current.cell_at(cursor_world(current), row, col) || !m_model.legal(row, col))
    {
        return oryx::PENDING_ACTION;
    }
    return TicTacToeBoardModel::action_for(row, col);
}

void TicTacToeGraphicsBoard::draw_status(const TicTacToeLayout& layout) const
{
    oryx::Font& font = oryx::Renderer::default_font();
    if (!font.ready())
    {
        return;
    }

    std::string text = m_model.status_text();
    float pixel_height = TicTacToeLayout::kStatusHeight;
    oryx::Colour colour = kStatusColour;
    if (m_model.terminal())
    {
        text += std::string(" - ") + oryx::kRestartHint;
        pixel_height *= 0.7f;
    }
    else
    {
        colour = mark_colour(TicTacToeBoardModel::mark_of(m_model.current_player()));
    }

    oryx::TextStyle style;
    style.pixel_height = pixel_height;
    style.colour = colour;
    style.align = oryx::TextAlign::Centre;
    oryx::Renderer::draw_text({ layout.viewport[0] * 0.5f, layout.viewport[1] - 2.0f * TicTacToeLayout::kStatusHeight - font.ascent(pixel_height) * 0.5f }, text, font, style);
}

void TicTacToeGraphicsBoard::render(double)
{
    TicTacToeLayout current = layout();
    if (current.viewport[0] <= 0.0f || current.viewport[1] <= 0.0f)
    {
        return;
    }

    oryx::Camera2D camera(current.viewport[0], current.viewport[1]);
    camera.set_position({ current.viewport[0] * 0.5f, current.viewport[1] * 0.5f });
    oryx::Renderer::begin_scene(camera);

    const float half = current.cell * 1.5f;
    for (float offset : { -0.5f, 0.5f })
    {
        oryx::Renderer::draw_rect({ current.centre[0] + offset * current.cell, current.centre[1] }, { kGridThickness, 2.0f * half }, kGridColour);
        oryx::Renderer::draw_rect({ current.centre[0], current.centre[1] + offset * current.cell }, { 2.0f * half, kGridThickness }, kGridColour);
    }

    size_t hover_row = 0;
    size_t hover_col = 0;
    if (!m_model.terminal() && current.cell_at(cursor_world(current), hover_row, hover_col) && m_model.legal(hover_row, hover_col))
    {
        oryx::Renderer::debug().rect(current.cell_centre(hover_row, hover_col), { current.cell, current.cell }, kHoverColour);
    }

    const float reach = current.cell * (0.5f - kMarkInset);
    for (size_t row = 0; row < TicTacToeBoardModel::kSize; ++row)
    {
        for (size_t col = 0; col < TicTacToeBoardModel::kSize; ++col)
        {
            Mark mark = m_model.mark_at(row, col);
            oryx::Vec2f centre = current.cell_centre(row, col);
            if (mark == Mark::X)
            {
                oryx::Renderer::draw_rect(centre, { kMarkThickness, 2.8f * reach }, mark_colour(mark), 0.7853982f);
                oryx::Renderer::draw_rect(centre, { kMarkThickness, 2.8f * reach }, mark_colour(mark), -0.7853982f);
            }
            else if (mark == Mark::O)
            {
                oryx::Renderer::draw_circle(centre, reach * 1.1f, mark_colour(mark), 0.2f);
            }
        }
    }

    draw_status(current);

    oryx::Renderer::draw_debug();
    oryx::Renderer::end_scene();
}

OX_REGISTER_GRAPHICS_BOARD(TicTacToeGraphicsBoard, "tictactoe")

} // namespace oasis
