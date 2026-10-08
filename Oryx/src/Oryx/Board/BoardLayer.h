#pragma once

#include "Oryx/Board/BoardSession.h"
#include "Oryx/Board/Selection.h"
#include "Oryx/Containers/SmallVector.h"
#include "Oryx/Core/Layer.h"
#include "Oryx/Core/Random.h"
#include "Oryx/Strategy/Observability/IDecisionObserver.h"

namespace oryx
{

class StartMatchEvent;

struct BoardLayerDesc
{
    // Empty picks the default (the terminal asks when it can).
    std::string game;
    // A strategy name, k_human_opponent for hot-seat, or empty to pick the default (the terminal asks when it can).
    std::string opponent;
    // Makes the board; empty plays in the terminal. A windowed board comes from here, with whatever feeds it frames registered by the factory itself.
    BoardFactory create_board;
    // The board reads stdin: choices are prompted for, the outcome is printed, and the application closes when stdin runs out.
    bool terminal = true;
    // Called with the old board when a new match replaces it, and at detach.
    BoardReleaser release_board;
    // Non-owning; sees every decision of every match this layer starts and must outlive the layer.
    IDecisionObserver* decision_observer = nullptr;
};

// Plays one game with a human against a strategy (or another human) through the game's board; knows no game by name, and no window, device or renderer.
// It owns what every board shares: the outcome announcement, restarting a finished game when the board asks (against a strategy, the human's seat, and so who moves first, is drawn again for each game), and quitting when stdin runs out.
class BoardLayer : public Layer
{
public:
    explicit BoardLayer(BoardLayerDesc desc);

    void attach() override;
    void detach() override;
    void update(double delta_time) override;
    // StartMatchEvent is only recorded here; update() applies it, never in the middle of a frame.
    void event(Event& event) override;

    // The match being played; empty before the first start.
    [[nodiscard]] const std::string& game_name() const { return m_game_name; }
    [[nodiscard]] const std::string& opponent_name() const { return m_opponent_name; }
    // A game that is not over and has had at least one move, so switching away would lose progress.
    [[nodiscard]] bool match_in_progress() const { return m_session != nullptr && !m_session->game_over() && m_session->moves() > 0; }

private:
    bool on_start_match(StartMatchEvent& event);
    void start(const std::string& game_name, const std::string& opponent_name);
    void apply_pending_match();
    void release_current_board();
    // Draws the human's seat for the next game and returns the strategy for each seat that goes with it.
    [[nodiscard]] SmallVector<uint32_t, 2> draw_next_seats();

    std::string m_requested_game;
    std::string m_requested_opponent;
    BoardFactory m_create_board;
    BoardReleaser m_release_board;
    IDecisionObserver* m_decision_observer;
    bool m_terminal;
    std::string m_game_name;
    std::string m_opponent_name;
    bool m_switch_requested = false;
    std::string m_pending_game;
    std::string m_pending_opponent;
    SharedPtr<BoardSession> m_session;
    Random m_random;
    // The human's player, or k_all_seats in hot-seat; `m_seat_order[seat]` is the index among the started strategies that plays `seat`, and the human's is `m_human_slot`.
    PlayerId m_human_seat = k_all_seats;
    uint32_t m_human_slot = 0;
    SmallVector<uint32_t, 2> m_seat_order;
};

} // namespace oryx
