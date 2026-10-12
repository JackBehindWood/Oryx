#include "SyntheticTreeObserver.h"

namespace oasis
{

namespace
{

using namespace oryx;

constexpr size_t k_max_moves = 12;
constexpr int64_t k_root_visits = 1000;

int64_t share_of(double weight, int64_t visits)
{
    const int64_t share = static_cast<int64_t>(weight * static_cast<double>(visits));
    return share < 1 ? 1 : share;
}

// Preorder, so a capped copy keeps a connected prefix; replies reuse the same action ids as the root moves, which is enough to look like a game.
void add_synthetic_tree(Decision& decision, uint64_t seed)
{
    const size_t moves = math::min(decision.scores.size(), k_max_moves);
    if (moves == 0)
    {
        return;
    }
    decision.tree.push_back(SearchNode{ -1, INVALID_ACTION, k_root_visits, 0.0 });
    for (size_t i = 0; i < moves; ++i)
    {
        const ActionScore& score = decision.scores[i];
        const double weight = score.has_probability ? score.probability : 1.0 / static_cast<double>(moves);
        const double wobble = std::sin(static_cast<double>(seed + i * 7));
        const int32_t move = static_cast<int32_t>(decision.tree.size());
        decision.tree.push_back(SearchNode{ 0, score.action, share_of(weight, k_root_visits), score.has_value ? score.value : weight * 2.0 - 1.0 + 0.1 * wobble });
        const int64_t visits = decision.tree.back().visits;
        const double value = decision.tree.back().value;
        for (size_t reply = 0; reply < 2 && reply + 1 < moves; ++reply)
        {
            const int32_t node = static_cast<int32_t>(decision.tree.size());
            decision.tree.push_back(SearchNode{ move, decision.scores[(i + reply + 1) % moves].action, share_of(reply == 0 ? 0.6 : 0.3, visits), -value + 0.05 * wobble });
            decision.tree.push_back(SearchNode{ node, decision.scores[(i + reply + 2) % moves].action, share_of(0.5, decision.tree[static_cast<size_t>(node)].visits), value });
        }
    }
}

} // namespace

void SyntheticTreeObserver::on_decision(const IState& state, const Decision& decision)
{
    if (!m_enabled || !decision.tree.empty())
    {
        m_target.on_decision(state, decision);
        return;
    }
    m_decorated = decision;
    add_synthetic_tree(m_decorated, m_count++);
    m_target.on_decision(state, m_decorated);
}

} // namespace oasis
