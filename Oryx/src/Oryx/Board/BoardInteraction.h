#pragma once

#include "Oryx/Board/BoardPresentation.h"

namespace oryx
{

// What a person does to a presented board, as intents with no devices or window in them: front ends turn mouse, keys or typed words into these calls
// and draw scene(). It owns the presentation, the move being built, the drag and the move waiting for poll().
class BoardInteraction
{
public:
    BoardInteraction(UniquePtr<IBoardPresenter> presenter, std::string game, PlayerId seat);

    // Describes the state again; a change drops the waiting move and any drag. True when anything shown or playable changed.
    bool update(const IState& state);

    void hover(SpaceId space) { m_hovered = space; }
    void set_cursor(const Vec2f& cursor) { m_cursor = cursor; }

    // Picks the space and, while the move stays open, starts a drag from it. No space, or one that starts no move, drops the picks.
    void press(SpaceId space);
    // Ends a drag: over another space the move can continue to, it picks it (drag-and-drop); over the same space it was a click and the picks stay.
    void release(SpaceId space);
    // Picks the `index`th entry of scene().options (an option or Confirm).
    void choose_option(size_t index);
    void confirm();
    // Takes back the last pick and ends any drag.
    void back();
    // Drops the picks and asks for the last move to be taken back.
    void undo();
    // Drops the picks and any drag.
    void cancel();
    // Takes one pick, with no drag: how typed words arrive.
    void submit(const Pick& pick);

    // The move or UNDO_ACTION a person finished since the last call, else PENDING_ACTION.
    ActionId poll();

    // Rebuilt on every call from the current hover, cursor and drag; the reference stays valid until the next call.
    [[nodiscard]] const BoardScene& scene();
    [[nodiscard]] const BoardPresentation& presentation() const { return m_presentation; }
    [[nodiscard]] bool accepts_moves() const { return m_presentation.accepts_moves(); }
    [[nodiscard]] bool dragging() const { return m_drag; }
    [[nodiscard]] SpaceId hovered() const { return m_hovered; }

private:
    PickStatus take(const Pick& pick);
    [[nodiscard]] static Pick space_pick(SpaceId space) { return { PickKind::Space, space, {} }; }

    BoardPresentation m_presentation;
    BoardScene m_scene;
    ActionId m_queued = PENDING_ACTION;
    SpaceId m_hovered = k_no_space;
    SpaceId m_drag_space = k_no_space;
    Vec2f m_cursor;
    bool m_drag = false;
};

} // namespace oryx
