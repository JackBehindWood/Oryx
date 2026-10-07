#include "PresentedGraphicsBoard2D.h"

#include "Oryx/Interface/Canvas/Replay.h"
#include "Oryx/Renderer/Renderer.h"

namespace oryx
{

PresentedGraphicsBoard2D::PresentedGraphicsBoard2D(UniquePtr<IBoardPresenter> presenter, std::string game, PlayerId seat, BoardTheme2D theme)
    : m_interaction(std::move(presenter), game, seat)
    , m_game(std::move(game))
    , m_theme(theme)
{
    apply_ui_theme(m_theme.font);
}

void PresentedGraphicsBoard2D::apply_ui_theme(Font* font)
{
    UiTheme theme = m_theme.ui;
    theme.font = font;
    m_ui.set_theme(theme);
}

void PresentedGraphicsBoard2D::refresh_ui_theme()
{
    Font* font = m_theme.font != nullptr ? m_theme.font : Renderer::initialised() ? &Renderer::default_font() : nullptr;
    if (font != m_ui.theme().font)
    {
        apply_ui_theme(font);
    }
}

void PresentedGraphicsBoard2D::refresh_text(const BoardScene& scene)
{
    const BoardPresentation& presentation = m_interaction.presentation();
    const bool terminal = presentation.terminal();
    const PlayerId to_move = presentation.to_move();
    const PlayerId seat = presentation.seat();
    const PlayerId winner = presentation.winner();
    if (terminal == m_text_terminal && to_move == m_text_to_move && seat == m_text_seat && winner == m_text_winner && scene.status == m_text_source)
    {
        return;
    }
    m_text_terminal = terminal;
    m_text_to_move = to_move;
    m_text_seat = seat;
    m_text_winner = winner;
    m_text_source = scene.status;

    m_status.clear();
    m_headline.clear();
    m_status_variant = {};
    m_headline_variant = {};
    const bool hot_seat = seat == k_all_seats;
    if (terminal)
    {
        m_headline_variant = winner < 0 ? "draw" : (hot_seat || winner == seat) ? "win" : "lose";
        if (hot_seat)
        {
            m_headline = scene.status;
        }
        else
        {
            m_headline = winner < 0 ? "Draw" : winner == seat ? "You win!" : "You lose";
        }
    }
    else if (hot_seat)
    {
        m_status = scene.status;
    }
    else
    {
        const bool mine = to_move == seat;
        m_status = (mine ? "Your turn - " : "Opponent's turn - ") + scene.status;
        m_status_variant = mine ? "your_turn" : "their_turn";
    }
}

void PresentedGraphicsBoard2D::reset(PlayerId seat)
{
    m_interaction.reset(seat);
    m_overlay = {};
    m_text_source.clear();
    m_text_to_move = -2;
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

void PresentedGraphicsBoard2D::update(const BoardInput& input, double delta_time)
{
    const BoardScene& scene = m_interaction.scene();
    if (scene.layout == nullptr)
    {
        m_interaction.hover(k_no_space);
        return;
    }
    refresh_ui_theme();
    refresh_text(scene);
    const BoardOverlayText text{ m_status, m_status_variant, m_headline, m_headline_variant, k_restart_hint };
    run_board_overlay(m_ui, input, delta_time, m_interaction.dragging(), scene, text, m_overlay);
    if (m_overlay.restart)
    {
        request_restart();
    }
    BoardProjection2D current = fit_board_2d(scene, input.viewport, m_overlay.board);
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
    if (m_overlay.chosen)
    {
        m_interaction.choose_option(m_overlay.option);
    }
    else if (input.select && !m_overlay.pointer_over_ui)
    {
        m_interaction.press(m_interaction.hovered());
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
    const bool overlay_current = m_overlay.viewport == input.viewport && !is_empty(m_overlay.board);
    m_projection = overlay_current ? fit_board_2d(scene, input.viewport, m_overlay.board) : fit_board_2d(scene, input.viewport);
    if (m_projection.scale <= 0.0f)
    {
        return;
    }
    Renderer::scene().submit(*this);
}

void PresentedGraphicsBoard2D::render_stage(RenderStage stage, StageContext& context)
{
    Font& font = m_theme.font != nullptr ? *m_theme.font : Renderer::default_font();
    if (stage == RenderStage::Scene2D)
    {
        draw_board_2d(context.batcher_2d, m_interaction.scene(), m_projection, m_theme, font);
    }
    else if (stage == RenderStage::Overlay && m_overlay.viewport == m_projection.viewport)
    {
        replay(m_ui.draw_list(), context.batcher_2d, font, { { 0.0f, 0.0f }, m_projection.viewport });
    }
}

} // namespace oryx
