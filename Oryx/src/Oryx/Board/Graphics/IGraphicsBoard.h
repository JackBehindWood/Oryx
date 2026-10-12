#pragma once

#include "Oryx/Board/BoardAnnotation.h"
#include "Oryx/Board/BoardInput.h"
#include "Oryx/Board/IBoard.h"
#include "Oryx/Core/Registry.h"
#include "Oryx/Renderer/FrameClient.h"

namespace oryx
{

// What an application tells a board about the decision on show; a board that draws no annotations ignores it. The board copies what it needs, so the views only have to live for the call.
struct BoardAnnotationRequest
{
    const BoardAnnotations* annotations = nullptr;
    AnnotationFlags flags;
    // Show the on-board chips and legend.
    bool controls = false;
    // "Ply 7 · Player 2 · minimax".
    std::string_view legend;
    // The decision shown is not the latest one.
    bool ghost = false;
    // A space another panel points at; the board outlines it.
    SpaceId linked = k_no_space;
};

// A windowed board: poll_action never blocks, and it is a frame client of GraphicsLayer, which hands it the window's input once per frame.
// frame() turns that into one BoardInput (the only device mapping), closes the application on quit, and runs update then render; a restart
// input is held for BoardLayer to take. Boards never read Input or the window themselves, so update and render run and test without a window.
class IGraphicsBoard : public IBoard, public IFrameClient
{
public:
    // Reacts to the frame's input and queues any move for poll_action.
    virtual void update(const BoardInput& input, double delta_time) = 0;
    // Submits what on_turn captured to the frame's open scene (the board is a RenderSource); runs after update.
    virtual void render(const BoardInput& input) = 0;

    virtual void set_annotations(const BoardAnnotationRequest&) {}
    // The flags as the person left them (the on-board chips can change them between requests).
    [[nodiscard]] virtual AnnotationFlags annotation_flags() const { return {}; }
    [[nodiscard]] virtual SpaceId hovered_space() const { return k_no_space; }

    void frame(const FrameInfo& info) final;
    [[nodiscard]] bool take_restart_request() final;

protected:
    // For a board's own control (a button) to ask for what the restart input asks for.
    void request_restart() { m_restart_requested = true; }

private:
    bool m_restart_requested = false;
};

using GraphicsBoardRegistry = Registry<IGraphicsBoard>;

} // namespace oryx

#define OX_REGISTER_GRAPHICS_BOARD(Type, game) \
    OX_REGISTER_FACTORY(::oryx::IGraphicsBoard, Type, game)
