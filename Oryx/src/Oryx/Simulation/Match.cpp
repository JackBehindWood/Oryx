#include "Match.h"

#include "Oryx/Core/Assert.h"

namespace oryx
{

Match::Match(const IGame& game, std::vector<IStrategy*> strategies)
    : m_game(game)
    , m_strategies(std::move(strategies))
    , m_state(game.new_initial_state())
{
    OX_CORE_ASSERT(static_cast<int32_t>(m_strategies.size()) == game.num_players(),
                    "Match: strategies.size() must equal game.num_players()");
}

Context Match::build_context(const IGame& game, IState& state)
{
    Context context(state);
    if (IActionFeatures* features = game.action_features())
    {
        context.provide<IActionFeatures>(features);
    }
    return context;
}

std::vector<std::type_index> Match::missing_capabilities(const IStrategy& strategy, const Context& context)
{
    std::vector<std::type_index> missing;
    for (const std::type_index& capability : strategy.required_capabilities())
    {
        if (!context.has_capability(capability))
        {
            missing.push_back(capability);
        }
    }
    return missing;
}

IStrategy& Match::current_strategy() const
{
    return *m_strategies[static_cast<size_t>(current_player())];
}

ActionId Match::decide() const
{
    IStrategy& strategy = current_strategy();
    Context context = build_context(m_game, *m_state);
    OX_CORE_ASSERT(missing_capabilities(strategy, context).empty(),
                    "Match: current strategy requires a capability the game does not provide.");
    return strategy.decide(context);
}

void Match::apply(ActionId action)
{
    m_state->apply(action);
    m_history.record(action);
}

ActionId Match::undo()
{
    ActionId action = m_history.undo();
    m_state->undo(action);
    return action;
}

ActionId Match::redo()
{
    ActionId action = m_history.redo();
    m_state->apply(action);
    return action;
}

Outcome Match::play()
{
    while (!is_terminal())
    {
        apply(decide());
    }
    return outcome();
}

} // namespace oryx
