#include "MoveBuilder.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

namespace
{

bool is_prefix(const PickList& prefix, const PickList& of)
{
    return prefix.size() <= of.size() && std::equal(prefix.begin(), prefix.end(), of.begin());
}

} // namespace

void MoveBuilder::reset(std::vector<MoveCandidate> candidates)
{
    for (size_t index = 0; index < candidates.size(); ++index)
    {
        const MoveCandidate& candidate = candidates[index];
        if (candidate.picks.empty())
        {
            throw Error("Action " + to_string(candidate.action) + " has no picks");
        }
        for (size_t other = index + 1; other < candidates.size(); ++other)
        {
            if (is_prefix(candidate.picks, candidates[other].picks) || is_prefix(candidates[other].picks, candidate.picks))
            {
                throw Error("The picks of actions " + to_string(candidate.action) + " and " + to_string(candidates[other].action) + " are ambiguous: one starts with the other");
            }
        }
    }
    m_candidates = std::move(candidates);
    m_picked.clear();
}

bool MoveBuilder::continues(const MoveCandidate& candidate, const Pick& pick) const
{
    size_t depth = m_picked.size();
    return candidate.picks.size() > depth && is_prefix(m_picked, candidate.picks) && candidate.picks[depth] == pick;
}

ActionId MoveBuilder::extend(const Pick& pick)
{
    const MoveCandidate* match = nullptr;
    for (const MoveCandidate& candidate : m_candidates)
    {
        if (continues(candidate, pick))
        {
            match = &candidate;
            break;
        }
    }
    if (match == nullptr)
    {
        return INVALID_ACTION;
    }

    m_picked.push_back(match->picks[m_picked.size()]);
    for (const MoveCandidate& candidate : m_candidates)
    {
        if (candidate.picks == m_picked)
        {
            ActionId action = candidate.action;
            m_picked.clear();
            return action;
        }
    }
    return PENDING_ACTION;
}

ActionId MoveBuilder::pick(const Pick& pick)
{
    ActionId action = extend(pick);
    if (action != INVALID_ACTION || m_picked.empty())
    {
        return action;
    }

    PickList previous = std::move(m_picked);
    m_picked.clear();
    action = extend(pick);
    if (action == INVALID_ACTION)
    {
        m_picked = std::move(previous);
    }
    return action;
}

bool MoveBuilder::back()
{
    if (m_picked.empty())
    {
        return false;
    }
    m_picked.pop_back();
    return true;
}

PickList MoveBuilder::next_picks() const
{
    PickList next;
    for (const MoveCandidate& candidate : m_candidates)
    {
        if (candidate.picks.size() <= m_picked.size() || !is_prefix(m_picked, candidate.picks))
        {
            continue;
        }
        const Pick& pick = candidate.picks[m_picked.size()];
        if (std::find(next.begin(), next.end(), pick) == next.end())
        {
            next.push_back(pick);
        }
    }
    return next;
}

bool MoveBuilder::can_pick(const Pick& pick) const
{
    return std::any_of(m_candidates.begin(), m_candidates.end(), [&](const MoveCandidate& candidate) { return continues(candidate, pick); });
}

std::vector<MoveCandidate> collect_candidates(const IBoardPresenter& presenter, const IState& state)
{
    std::vector<MoveCandidate> candidates;
    if (state.is_terminal())
    {
        return candidates;
    }
    for (ActionId action : state.legal_actions())
    {
        MoveCandidate candidate;
        candidate.action = action;
        presenter.action_picks(state, action, candidate.picks);
        candidates.push_back(std::move(candidate));
    }
    return candidates;
}

} // namespace oryx
