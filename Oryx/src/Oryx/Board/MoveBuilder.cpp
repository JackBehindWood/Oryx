#include "MoveBuilder.h"

namespace oryx
{

namespace
{

const Pick k_confirm_pick = { PickKind::Confirm, 0, "confirm" };

} // namespace

void MoveBuilder::reset(const std::vector<MoveCandidate>& candidates)
{
    m_tree.build(candidates);
    m_path.clear();
    m_picked.clear();
    m_path.reserve(m_tree.max_depth());
    m_picked.reserve(m_tree.max_depth());
}

void MoveBuilder::clear()
{
    m_path.clear();
    m_picked.clear();
}

bool MoveBuilder::ready() const
{
    uint32_t node = current();
    return !m_path.empty() && m_tree.is_move(node) && m_tree.has_children(node);
}

uint32_t MoveBuilder::resolve(const Pick& pick, bool& restart) const
{
    restart = false;
    uint32_t node = m_tree.child(current(), pick);
    if (node != MoveTree::k_none || m_path.empty())
    {
        return node;
    }
    restart = true;
    return m_tree.child(MoveTree::k_root, pick);
}

PickResult MoveBuilder::pick(const Pick& pick)
{
    if (pick.kind == PickKind::Confirm)
    {
        return confirm();
    }

    bool restart = false;
    uint32_t node = resolve(pick, restart);
    if (node == MoveTree::k_none)
    {
        return {};
    }
    if (restart)
    {
        clear();
    }

    ActionId action = m_tree.node(node).action;
    if (action != INVALID_ACTION && !m_tree.has_children(node))
    {
        clear();
        return { PickStatus::Complete, action };
    }

    m_path.push_back(node);
    m_picked.push_back(m_tree.node(node).pick);
    return { action != INVALID_ACTION ? PickStatus::Ready : PickStatus::Pending, action };
}

PickResult MoveBuilder::confirm()
{
    if (!ready())
    {
        return {};
    }
    ActionId action = m_tree.node(current()).action;
    clear();
    return { PickStatus::Complete, action };
}

bool MoveBuilder::back()
{
    if (m_path.empty())
    {
        return false;
    }
    m_path.pop_back();
    m_picked.pop_back();
    return true;
}

void MoveBuilder::list_after(uint32_t node, PickList& out) const
{
    out.clear();
    for (uint32_t child = m_tree.node(node).first_child; child != MoveTree::k_none; child = m_tree.node(child).next_sibling)
    {
        out.push_back(m_tree.node(child).pick);
    }
}

PickList MoveBuilder::next_picks() const
{
    PickList next;
    next_picks(next);
    return next;
}

void MoveBuilder::next_picks(PickList& out) const
{
    list_after(current(), out);
    if (ready())
    {
        out.push_back(k_confirm_pick);
    }
}

PickList MoveBuilder::preview(const Pick& pick) const
{
    PickList out;
    preview(pick, out);
    return out;
}

void MoveBuilder::preview(const Pick& pick, PickList& out) const
{
    out.clear();
    if (pick.kind == PickKind::Confirm)
    {
        return;
    }

    bool restart = false;
    uint32_t node = resolve(pick, restart);
    if (node == MoveTree::k_none)
    {
        return;
    }
    list_after(node, out);
    if (!out.empty() && m_tree.is_move(node))
    {
        out.push_back(k_confirm_pick);
    }
}

bool MoveBuilder::can_pick(const Pick& pick) const
{
    if (pick.kind == PickKind::Confirm)
    {
        return ready();
    }
    bool restart = false;
    return resolve(pick, restart) != MoveTree::k_none;
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
        OX_CORE_ASSERT(is_game_action(action), "legal_actions() returned a reserved action id.");
        MoveCandidate candidate;
        candidate.action = action;
        presenter.action_picks(state, action, candidate.picks);
        candidates.push_back(std::move(candidate));
    }
    return candidates;
}

} // namespace oryx
