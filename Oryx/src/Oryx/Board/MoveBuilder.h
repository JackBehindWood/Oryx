#pragma once

#include "Oryx/Board/MoveTree.h"

namespace oryx
{

enum class PickStatus : uint8_t
{
    // No move continues that way; nothing changed.
    Rejected,
    // More picks are needed.
    Pending,
    // A legal move that may also continue: confirm() plays it, another pick extends it.
    Ready,
    // A move with nothing left to pick; it was played and the picks are cleared.
    Complete
};

struct PickResult
{
    PickStatus status = PickStatus::Rejected;
    // The move for Ready and Complete, INVALID_ACTION otherwise.
    ActionId action = INVALID_ACTION;
};

// Builds a move from a person's picks, one at a time, for any game: the legal moves are a prefix tree of pick sequences and the picks so far are a path in it.
// Front ends own no move logic; they turn clicks or typed words into picks and highlight next_picks().
class MoveBuilder
{
public:
    // Replaces the candidates and drops any picks. Throws Error if a candidate has no picks or a Confirm pick, or if two candidates' picks are equal.
    // One candidate's picks may start another's: the shorter is a move that can stop there or continue.
    void reset(const std::vector<MoveCandidate>& candidates);

    // A pick that does not continue the current picks but starts a move begins again from it (clicking another piece selects it instead).
    // A Confirm pick is confirm().
    PickResult pick(const Pick& pick);
    // Plays a Ready move; Rejected when the picks so far are not a complete move.
    PickResult confirm();
    // Drops the last pick; false when there was none.
    bool back();
    void clear();

    [[nodiscard]] const PickList& picked() const { return m_picked; }
    [[nodiscard]] bool empty() const { return m_tree.empty(); }
    // True when the picks so far are a legal move that confirm() would play.
    [[nodiscard]] bool ready() const;
    // The picks that continue the current ones, in candidate order, plus Confirm while ready().
    [[nodiscard]] PickList next_picks() const;
    void next_picks(PickList& out) const;
    // What next_picks() would hold after `pick`, without taking it; empty when pick would be rejected or complete the move.
    [[nodiscard]] PickList preview(const Pick& pick) const;
    void preview(const Pick& pick, PickList& out) const;
    // True when pick() would not reject it.
    [[nodiscard]] bool can_pick(const Pick& pick) const;

private:
    [[nodiscard]] uint32_t current() const { return m_path.empty() ? MoveTree::k_root : m_path.back(); }
    // The node `pick` leads to, from the current picks or else from the start; `restart` says it was the start.
    [[nodiscard]] uint32_t resolve(const Pick& pick, bool& restart) const;
    void list_after(uint32_t node, PickList& out) const;

    MoveTree m_tree;
    std::vector<uint32_t> m_path;
    PickList m_picked;
};

// Every legal action of `state` with its picks.
[[nodiscard]] std::vector<MoveCandidate> collect_candidates(const IBoardPresenter& presenter, const IState& state);

} // namespace oryx
