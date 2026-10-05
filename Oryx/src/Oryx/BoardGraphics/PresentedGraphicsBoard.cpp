#include "PresentedGraphicsBoard.h"

#include "Oryx/Renderer/Camera.h"
#include "Oryx/Renderer/Renderer.h"

namespace oryx
{

PresentedGraphicsBoard::PresentedGraphicsBoard(UniquePtr<IBoardPresenter> presenter, std::string game, PlayerId seat, BoardTheme2D theme)
    : m_presentation(std::move(presenter), std::move(game), seat)
    , m_theme(theme)
{
}

void PresentedGraphicsBoard::refresh(const IState& state)
{
    if (m_presentation.update(state))
    {
        m_queued = PENDING_ACTION;
    }
}

void PresentedGraphicsBoard::on_turn(const IState& state)
{
    refresh(state);
}

ActionId PresentedGraphicsBoard::poll_action(const IState& state)
{
    refresh(state);
    ActionId action = m_queued;
    m_queued = PENDING_ACTION;
    return action;
}

BoardLayout2D PresentedGraphicsBoard::layout(const BoardInput& input)
{
    m_presentation.build_scene(m_hovered, m_scene);
    return fit_board_2d(m_scene, input.viewport);
}

void PresentedGraphicsBoard::update(const BoardInput& input, double)
{
    BoardLayout2D current = layout(input);
    m_hovered = space_at(m_presentation.view(), cursor_to_board(current, input.cursor));
    if (!m_presentation.accepts_moves())
    {
        return;
    }

    MoveBuilder& builder = m_presentation.builder();
    if (input.undo)
    {
        builder.clear();
        m_queued = UNDO_ACTION;
        return;
    }
    if (input.back)
    {
        builder.back();
        return;
    }
    if (!input.select)
    {
        return;
    }

    ActionId action = INVALID_ACTION;
    std::vector<OptionButton2D> buttons;
    option_buttons_2d(current, m_scene.options.size(), buttons);
    size_t option = option_at(buttons, current, input.cursor);
    if (option < buttons.size())
    {
        action = builder.pick(m_scene.options[option]);
    }
    else if (m_hovered != kNoSpace)
    {
        action = builder.pick({ PickKind::Space, m_hovered, {} });
    }

    if (action == INVALID_ACTION)
    {
        builder.clear();
    }
    else if (action != PENDING_ACTION)
    {
        m_queued = action;
    }
}

void PresentedGraphicsBoard::render(const BoardInput& input)
{
    BoardLayout2D current = layout(input);
    if (current.scale <= 0.0f)
    {
        return;
    }

    std::string status;
    if (m_presentation.terminal())
    {
        status = m_scene.status + " - " + kRestartHint;
    }

    Camera2D camera(input.viewport[0], input.viewport[1]);
    camera.set_position({ input.viewport[0] * 0.5f, input.viewport[1] * 0.5f });
    Renderer::begin_scene(camera);
    draw_board_2d(m_scene, current, m_theme, Renderer::default_font(), status);
    Renderer::end_scene();
}

} // namespace oryx
