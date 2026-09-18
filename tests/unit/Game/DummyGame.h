#pragma once

#include "Oryx.h"

namespace oryx::test
{

// Misere Nim fixture (not a real game): distinct from Phase 3's Tic-Tac-Toe
// by design, so it doesn't overlap. ActionId N means "take N stones".
class DummyState : public IState
{
public:
    explicit DummyState(uint32_t pile_size) : m_pile(pile_size) {}

    std::vector<ActionId> legal_actions() const override
    {
        std::vector<ActionId> actions;
        for (ActionId take = 1; take <= 3 && take <= m_pile; ++take)
        {
            actions.push_back(take);
        }
        return actions;
    }

    void apply(ActionId action) override
    {
        m_pile -= action;
        m_current_player = 1 - m_current_player;
    }

    void undo(ActionId action) override
    {
        m_pile += action;
        m_current_player = 1 - m_current_player;
    }

    PlayerId current_player() const override { return m_current_player; }

    bool is_terminal() const override { return m_pile == 0; }

    Outcome outcome() const override
    {
        Outcome result;
        result.is_terminal = is_terminal();
        result.rewards = Rewards<double>(2);
        if (result.is_terminal)
        {
            // The player who took the last stone loses.
            PlayerId winner = m_current_player;
            PlayerId loser = 1 - m_current_player;
            result.rewards[winner] = 1.0;
            result.rewards[loser] = -1.0;
        }
        return result;
    }

    std::string action_to_string(ActionId action) const override
    {
        return "take " + oryx::to_string(action);
    }

private:
    uint32_t m_pile;
    PlayerId m_current_player = 0;
};

class DummyGame : public IGame
{
public:
    explicit DummyGame(uint32_t pile_size = 10) : m_pile_size(pile_size) {}

    UniquePtr<IState> new_initial_state() const override
    {
        return create_unique<DummyState>(m_pile_size);
    }

    std::string name() const override { return "DummyTakeAway"; }
    int32_t num_players() const override { return 2; }

private:
    uint32_t m_pile_size;
};

class DummyGreedyStrategy : public IStrategy
{
public:
    ActionId decide(const Context& context) override
    {
        auto actions = context.state().legal_actions();
        return *std::max_element(actions.begin(), actions.end());
    }
};

} // namespace oryx::test
