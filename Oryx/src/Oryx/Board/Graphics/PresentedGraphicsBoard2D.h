#pragma once

#include "Oryx/Board/BoardInteraction.h"
#include "Oryx/Board/Graphics/IGraphicsBoard.h"
#include "Oryx/Board/Graphics/BoardOverlay2D.h"
#include "Oryx/Board/Graphics/BoardRenderer2D.h"
#include "Oryx/Renderer/Scene/SceneRenderer.h"

namespace oryx
{

// The 2D windowed front end for any game with an IBoardPresenter: the mouse and keys become BoardInteraction intents, and it is a RenderSource that draws the scene
// (board in Scene2D; the UI-built turn line, buttons and result banner in Overlay) when render() submits it to the frame's scene.
class PresentedGraphicsBoard2D : public IGraphicsBoard, public RenderSource
{
public:
    PresentedGraphicsBoard2D(UniquePtr<IBoardPresenter> presenter, std::string game, PlayerId seat, BoardTheme2D theme = {});

    void on_turn(const IState& state) override;
    void reset(PlayerId seat) override;
    ActionId poll_action(const IState& state) override;
    void update(const BoardInput& input, double delta_time) override;
    void render(const BoardInput& input) override;
    void render_stage(RenderStage stage, StageContext& context) override;
    bool shows_moves() const override { return true; }

    [[nodiscard]] const BoardPresentation& presentation() const { return m_interaction.presentation(); }
    [[nodiscard]] SpaceId hovered() const { return m_interaction.hovered(); }
    [[nodiscard]] bool dragging() const { return m_interaction.dragging(); }
    // What the last update's overlay frame found: the board area and the option buttons' rects.
    [[nodiscard]] const BoardOverlayResult& overlay() const { return m_overlay; }
    [[nodiscard]] const UiContext& ui() const { return m_ui; }

private:
    void refresh_text(const BoardScene& scene);
    void refresh_ui_theme();
    void apply_ui_theme(Font* font);

    BoardInteraction m_interaction;
    std::string m_game;
    BoardTheme2D m_theme;
    BoardProjection2D m_projection;
    UiContext m_ui;
    BoardOverlayResult m_overlay;
    std::string m_status;
    std::string_view m_status_variant;
    std::string m_headline;
    std::string_view m_headline_variant;
    std::string m_text_source;
    PlayerId m_text_to_move = -2;
    PlayerId m_text_seat = -2;
    PlayerId m_text_winner = -2;
    bool m_text_terminal = false;
};

} // namespace oryx
