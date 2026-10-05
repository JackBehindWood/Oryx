#pragma once

#include "Oryx/Board/IBoardPresenter.h"

namespace oryx
{

struct MoveCandidate
{
    ActionId action = INVALID_ACTION;
    PickList picks;
};

// Builds a move from a person's picks, one at a time, for any game: it keeps the legal moves whose picks start with what was picked so far.
// Front ends own no move logic; they turn clicks or typed words into picks and highlight next_picks().
class MoveBuilder
{
public:
    // Replaces the candidates and drops any picks. Throws Error if a candidate has no picks, or if two candidates' picks are equal or one is a prefix of another.
    void reset(std::vector<MoveCandidate> candidates);

    // The action once `pick` completes one, PENDING_ACTION while more picks are needed, INVALID_ACTION if no move starts that way.
    // A pick that does not continue the current picks but starts a move begins again from it (clicking another piece selects it instead).
    ActionId pick(const Pick& pick);
    // Drops the last pick; false when there was none.
    bool back();
    void clear() { m_picked.clear(); }

    [[nodiscard]] const PickList& picked() const { return m_picked; }
    [[nodiscard]] bool empty() const { return m_candidates.empty(); }
    // The distinct picks that continue the current ones, in candidate order.
    [[nodiscard]] PickList next_picks() const;
    [[nodiscard]] bool can_pick(const Pick& pick) const;

private:
    [[nodiscard]] bool continues(const MoveCandidate& candidate, const Pick& pick) const;
    [[nodiscard]] ActionId extend(const Pick& pick);

    std::vector<MoveCandidate> m_candidates;
    PickList m_picked;
};

// Every legal action of `state` with its picks.
[[nodiscard]] std::vector<MoveCandidate> collect_candidates(const IBoardPresenter& presenter, const IState& state);

} // namespace oryx
