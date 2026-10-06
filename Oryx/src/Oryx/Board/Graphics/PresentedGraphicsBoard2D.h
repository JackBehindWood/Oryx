#pragma once

#include "Oryx/Board/BoardInteraction.h"
#include "Oryx/Board/Graphics/IGraphicsBoard.h"
#include "Oryx/Board/Graphics/BoardRenderer2D.h"
#include "Oryx/Renderer/Scene/SceneRenderer.h"

namespace oryx
{

// The 2D windowed front end for any game with an IBoardPresenter: the mouse and keys become BoardInteraction intents, and it is a RenderSource that draws the scene
// (board in Scene2D, buttons and status in Overlay) when render() submits it to the frame's scene.
class PresentedGraphicsBoard2D : public IGraphicsBoard, public RenderSource
{
public:
    PresentedGraphicsBoard2D(UniquePtr<IBoardPresenter> presenter, std::string game, PlayerId seat, BoardTheme2D theme = {});

    void on_turn(const IState& state) override;
    ActionId poll_action(const IState& state) override;
    void update(const BoardInput& input, double delta_time) override;
    void render(const BoardInput& input) override;
    void render_stage(RenderStage stage, StageContext& context) override;
    bool shows_moves() const override { return true; }

    [[nodiscard]] const BoardPresentation& presentation() const { return m_interaction.presentation(); }
    [[nodiscard]] SpaceId hovered() const { return m_interaction.hovered(); }
    [[nodiscard]] bool dragging() const { return m_interaction.dragging(); }

private:
    BoardInteraction m_interaction;
    std::string m_game;
    BoardTheme2D m_theme;
    BoardProjection2D m_projection;
    std::vector<OptionButton2D> m_buttons;
    std::string m_status;
    std::string m_status_source;
    bool m_status_terminal = false;
};

} // namespace oryx
