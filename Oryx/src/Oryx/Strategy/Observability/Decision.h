#pragma once

#include "Oryx/Containers/SmallVector.h"
#include "Oryx/Core/Metrics.h"
#include "Oryx/Game/ActionId.h"
#include "Oryx/Game/PlayerId.h"

namespace oryx
{

using Diagnostics = Metrics;

struct ActionScore
{
    ActionId action = INVALID_ACTION;
    double probability = 0.0;
    double value = 0.0;
    bool has_probability = false;
    bool has_value = false;
};

// Flat list entry so a tree from any search (alpha-beta, MCTS) fits without a fixed schema; parent is an index into the list, -1 for the root.
struct SearchNode
{
    int32_t parent = -1;
    ActionId action = INVALID_ACTION;
    int64_t visits = 0;
    double value = 0.0;
};

struct Decision
{
    PlayerId player = 0;
    ActionId chosen = INVALID_ACTION;
    SmallVector<ActionScore, k_action_list_inline_capacity> scores;
    Diagnostics extra;
    std::vector<SearchNode> tree;
};

inline ActionScore& score_for(Decision& decision, ActionId action)
{
    for (ActionScore& score : decision.scores)
    {
        if (score.action == action)
        {
            return score;
        }
    }
    decision.scores.push_back(ActionScore{});
    decision.scores.back().action = action;
    return decision.scores.back();
}

inline void set_probability(Decision& decision, ActionId action, double probability)
{
    ActionScore& score = score_for(decision, action);
    score.probability = probability;
    score.has_probability = true;
}

inline void set_value(Decision& decision, ActionId action, double value)
{
    ActionScore& score = score_for(decision, action);
    score.value = value;
    score.has_value = true;
}

} // namespace oryx
