#include "PresentedGraphicsBoard2D.h"

#include "Oryx/Renderer/Camera.h"
#include "Oryx/Renderer/Renderer.h"

namespace oryx
{

PresentedGraphicsBoard2D::PresentedGraphicsBoard2D(UniquePtr<IBoardPresenter> presenter, std::string game, PlayerId seat, BoardTheme2D theme)
    : m_interaction(std::move(presenter), game, seat)
    , m_game(std::move(game))
    , m_theme(theme)
{
}

void PresentedGraphicsBoard2D::on_turn(const IState& state)
{
    m_interaction.update(state);
}

ActionId PresentedGraphicsBoard2D::poll_action(const IState& state)
{
    m_interaction.update(state);
    return m_interaction.poll();
}

void PresentedGraphicsBoard2D::update(const BoardInput& input, double)
{
    const BoardScene& scene = m_interaction.scene();
    if (scene.layout == nullptr)
    {
        m_interaction.hover(k_no_space);
        return;
    }
    BoardProjection2D current = fit_board_2d(scene, input.viewport);
    m_interaction.hover(space_at(layout_as<BoardLayout2D>(scene.layout, m_game), cursor_to_board(current, input.cursor)));
    m_interaction.set_cursor(input.cursor);
    if (!m_interaction.accepts_moves())
    {
        return;
    }

    if (input.undo)
    {
        m_interaction.undo();
        return;
    }
    if (input.back)
    {
        m_interaction.back();
        return;
    }
    if (input.confirm)
    {
        m_interaction.confirm();
    }
    if (input.select)
    {
        option_buttons_2d(current, scene.options.size(), m_buttons);
        size_t option = option_at(m_buttons, current, input.cursor);
        if (option < m_buttons.size())
        {
            m_interaction.choose_option(option);
        }
        else
        {
            m_interaction.press(m_interaction.hovered());
        }
    }
    if (input.select_released)
    {
        m_interaction.release(m_interaction.hovered());
    }
    else if (!input.select_down && m_interaction.dragging() && !input.select)
    {
        m_interaction.release(k_no_space);
    }
}

void PresentedGraphicsBoard2D::render(const BoardInput& input)
{
    const BoardScene& scene = m_interaction.scene();
    BoardProjection2D current = fit_board_2d(scene, input.viewport);
    if (current.scale <= 0.0f)
    {
        return;
    }

    bool terminal = m_interaction.presentation().terminal();
    if (terminal != m_status_terminal || scene.status != m_status_source)
    {
        m_status_terminal = terminal;
        m_status_source = scene.status;
        m_status = terminal ? scene.status + " - " + k_restart_hint : std::string();
    }

    Camera2D camera(input.viewport[0], input.viewport[1]);
    camera.set_position({ input.viewport[0] * 0.5f, input.viewport[1] * 0.5f });
    Renderer::begin_scene(camera);
    draw_board_2d(scene, current, m_theme, Renderer::default_font(), m_status);
    Renderer::end_scene();
}

} // namespace oryx
