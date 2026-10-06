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

TEST_CASE("Every registered strategy returns a legal action in the initial state of every registered game it plays")
{
    for (const std::string& game_name : GameRegistry::names())
    {
        UniquePtr<IGame> game = try_create_game(game_name);
        if (game == nullptr)
        {
            continue;
        }
        for (const std::string& strategy_name : selection::strategies_for(game_name))
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

namespace
{

struct PresenterSnapshot
{
    SharedPtr<const BoardLayout> layout;
    BoardContent content;
    std::vector<PickList> picks;
};

PresenterSnapshot snapshot(const IBoardPresenter& presenter, const IState& state)
{
    PresenterSnapshot shot;
    shot.layout = presenter.layout(state);
    presenter.describe_pieces(state, state.current_player(), shot.content);
    if (!state.is_terminal())
    {
        for (ActionId action : state.legal_actions())
        {
            PickList picks;
            presenter.action_picks(state, action, picks);
            shot.picks.push_back(std::move(picks));
        }
    }
    return shot;
}

bool same(const PresenterSnapshot& a, const PresenterSnapshot& b)
{
    return a.layout == b.layout && a.content.pieces == b.content.pieces && a.content.status == b.content.status && a.picks == b.picks;
}

} // namespace

TEST_CASE("Every registered presenter describes its game's states validly, and the picks of each legal action play exactly that action")
{
    for (const std::string& name : BoardPresenterRegistry::names())
    {
        CAPTURE(name);
        UniquePtr<IGame> game = try_create_game(name);
        REQUIRE(game != nullptr);
        UniquePtr<IBoardPresenter> presenter = BoardPresenterRegistry::create(name);
        UniquePtr<IState> state = game->new_initial_state();

        for (size_t ply = 0;; ++ply)
        {
            BoardView view;
            view.layout = presenter->layout(*state);
            presenter->describe_pieces(*state, state->current_player(), view);
            CHECK_NOTHROW(validate_view(view, name));
            CHECK_FALSE(view.status.empty());
            if (state->is_terminal())
            {
                break;
            }

            std::vector<MoveCandidate> candidates = collect_candidates(*presenter, *state);
            MoveBuilder builder;
            REQUIRE_NOTHROW(builder.reset(candidates));
            for (const MoveCandidate& candidate : candidates)
            {
                ActionId played = INVALID_ACTION;
                for (const Pick& pick : candidate.picks)
                {
                    CHECK((pick.kind != PickKind::Space || pick.value < view.layout->space_count()));
                    CHECK((pick.kind != PickKind::Option || !pick.label.empty()));
                    played = builder.pick(pick).action;
                }
                CHECK(played == candidate.action);
            }

            ActionList legal = state->legal_actions();
            state->apply(legal[ply % legal.size()]);
        }
    }
}

TEST_CASE("Every registered presenter is stateless: layout, pieces and picks repeat over repeated and reversed states")
{
    for (const std::string& name : BoardPresenterRegistry::names())
    {
        CAPTURE(name);
        UniquePtr<IGame> game = try_create_game(name);
        REQUIRE(game != nullptr);
        UniquePtr<IBoardPresenter> presenter = BoardPresenterRegistry::create(name);

        std::vector<ActionId> played;
        std::vector<PresenterSnapshot> shots;
        UniquePtr<IState> state = game->new_initial_state();
        for (size_t ply = 0;; ++ply)
        {
            shots.push_back(snapshot(*presenter, *state));
            if (state->is_terminal())
            {
                break;
            }
            ActionList legal = state->legal_actions();
            played.push_back(legal[ply % legal.size()]);
            state->apply(played.back());
        }

        for (size_t pass = 0; pass < 2; ++pass)
        {
            for (size_t index = shots.size(); index-- > 0;)
            {
                UniquePtr<IState> replay = game->new_initial_state();
                for (size_t step = 0; step < index; ++step)
                {
                    replay->apply(played[step]);
                }
                CHECK(same(shots[index], snapshot(*presenter, *replay)));
                CHECK(shots[index].layout == shots.front().layout);
            }
        }
    }
}
