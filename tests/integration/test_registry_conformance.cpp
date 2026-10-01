#include "doctest.h"

#include "Oryx.h"

#include <algorithm>

using namespace oryx;

namespace
{

UniquePtr<IGame> try_create_game(const std::string& name)
{
    try
    {
        return create_game(name);
    }
    catch (const ParamError&)
    {
        return nullptr;
    }
}

bool is_legal(const IState& state, ActionId action)
{
    ActionList legal = state.legal_actions();
    return std::find(legal.begin(), legal.end(), action) != legal.end();
}

} // namespace

TEST_CASE("Every registered game starts non-terminal with a legal move and a valid player to move")
{
    for (const std::string& name : GameRegistry::names())
    {
        UniquePtr<IGame> game = try_create_game(name);
        if (game == nullptr)
        {
            continue;
        }
        CAPTURE(name);
        UniquePtr<IState> state = game->new_initial_state();

        CHECK_FALSE(state->is_terminal());
        CHECK_FALSE(state->legal_actions().empty());
        CHECK(game->num_players() > 0);
        CHECK(state->current_player() < game->num_players());
    }
}

TEST_CASE("Every registered game plays to a terminal state through legal moves, and undo restores each ply")
{
    for (const std::string& name : GameRegistry::names())
    {
        UniquePtr<IGame> game = try_create_game(name);
        if (game == nullptr)
        {
            continue;
        }
        CAPTURE(name);
        UniquePtr<IState> state = game->new_initial_state();

        while (!state->is_terminal())
        {
            ActionList legal = state->legal_actions();
            REQUIRE_FALSE(legal.empty());
            ActionId action = legal[0];
            CHECK_FALSE(state->action_to_string(action).empty());
            PlayerId mover = state->current_player();

            state->apply(action);
            state->undo(action);
            CHECK(state->current_player() == mover);
            CHECK(state->legal_actions().size() == legal.size());
            CHECK_FALSE(state->is_terminal());

            state->apply(action);
        }

        CHECK(state->outcome().is_terminal);
        CHECK(state->outcome().rewards.player_count() == static_cast<size_t>(game->num_players()));
    }
}

TEST_CASE("Every registered strategy returns a legal action in every registered game's initial state")
{
    for (const std::string& game_name : GameRegistry::names())
    {
        UniquePtr<IGame> game = try_create_game(game_name);
        if (game == nullptr)
        {
            continue;
        }
        for (const std::string& strategy_name : StrategyRegistry::names())
        {
            CAPTURE(game_name);
            CAPTURE(strategy_name);
            UniquePtr<IStrategy> strategy;
            try
            {
                strategy = StrategyRegistry::create(strategy_name);
            }
            catch (const ParamError&)
            {
                continue;
            }
            UniquePtr<IState> state = game->new_initial_state();
            Context context(*state);

            CHECK(is_legal(*state, strategy->decide(context)));
        }
    }
}
