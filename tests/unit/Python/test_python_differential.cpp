#include "doctest.h"

#include "unit/Python/PythonTestSupport.h"

using namespace oryx;
using namespace oryx::test;

#ifdef OX_ENABLE_PYTHON

namespace
{

constexpr const char* kCppNim = "nim-cpp";

class CppNimState : public IState
{
public:
    CppNimState(int64_t stones, int64_t max_take)
        : m_stones(stones)
        , m_max_take(max_take)
    {
    }

    ActionList legal_actions() const override
    {
        ActionList actions;
        for (int64_t take = 1; take <= std::min(m_max_take, m_stones); ++take)
        {
            actions.push_back(static_cast<ActionId>(take));
        }
        return actions;
    }

    void apply(ActionId action) override
    {
        m_stones -= action;
        m_player = 1 - m_player;
    }

    void undo(ActionId action) override
    {
        m_stones += action;
        m_player = 1 - m_player;
    }

    PlayerId current_player() const override { return m_player; }
    bool is_terminal() const override { return m_stones == 0; }

    Outcome outcome() const override
    {
        Outcome result;
        result.is_terminal = is_terminal();
        result.rewards = Rewards<double>(2);
        if (result.is_terminal)
        {
            result.rewards[0] = -1.0;
            result.rewards[1] = -1.0;
            result.rewards[1 - m_player] = 1.0;
        }
        return result;
    }

    std::string action_to_string(ActionId action) const override { return "take " + std::to_string(action); }

private:
    int64_t m_stones;
    int64_t m_max_take;
    PlayerId m_player = 0;
};

class CppNimGame : public IGame
{
public:
    CppNimGame(int64_t stones, int64_t max_take)
        : m_stones(stones)
        , m_max_take(max_take)
    {
    }

    UniquePtr<IState> new_initial_state() const override { return create_unique<CppNimState>(m_stones, m_max_take); }
    std::string name() const override { return "Nim (C++)"; }
    int32_t num_players() const override { return 2; }

private:
    int64_t m_stones;
    int64_t m_max_take;
};

class ScopedCppNim
{
public:
    ScopedCppNim()
    {
        GameRegistry::register_factory(kCppNim,
            [](const Params& params) -> UniquePtr<IGame>
            {
                return create_unique<CppNimGame>(get_param<int64_t>(params, "stones"), get_param<int64_t>(params, "max_take"));
            },
            EntryInfo{ { int_param("stones", 21), int_param("max_take", 3) }, "Nim ported to C++" });
    }

    ~ScopedCppNim() { GameRegistry::unregister_factory(kCppNim); }

    ScopedCppNim(const ScopedCppNim&) = delete;
    ScopedCppNim& operator=(const ScopedCppNim&) = delete;
};

Params nim_params(int64_t stones, int64_t max_take)
{
    return Params{ { "stones", stones }, { "max_take", max_take } };
}

BatchResult run_batch(const std::string& game_name, const Params& game_params, const std::string& strategy_a, const std::string& strategy_b, int32_t games)
{
    UniquePtr<IGame> game = create_game(game_name, game_params);

    SmallVector<UniquePtr<IStrategy>, 2> owned;
    SmallVector<IStrategy*, 2> seats;
    int64_t seed = 100;
    for (const std::string& name : { strategy_a, strategy_b })
    {
        Params params;
        if (name == "random")
        {
            params["seed"] = seed++;
        }
        owned.push_back(StrategyRegistry::create(name, params));
        seats.push_back(owned.back().get());
    }

    return BatchRunner(*game, seats).run(games);
}

void check_same_actions(const ActionList& python, const ActionList& cpp)
{
    REQUIRE(python.size() == cpp.size());
    for (size_t i = 0; i < python.size(); ++i)
    {
        CHECK(python[i] == cpp[i]);
    }
}

void check_same_outcome(const Outcome& python, const Outcome& cpp)
{
    CHECK(python.is_terminal == cpp.is_terminal);
    REQUIRE(python.rewards.player_count() == cpp.rewards.player_count());
    for (size_t player = 0; player < python.rewards.player_count(); ++player)
    {
        CHECK(python.rewards[static_cast<PlayerId>(player)] == cpp.rewards[static_cast<PlayerId>(player)]);
    }
}

struct Config
{
    int64_t stones;
    int64_t max_take;
    const char* strategy_a;
    const char* strategy_b;
    int32_t games;
};

} // namespace

TEST_CASE("a Python Nim and its C++ port produce identical batch results under identical seeds")
{
    ScopedCppNim cpp_nim;
    RunningPython python;
    python.load(repo_file("Oasis/scripts/nim.py"));
    python.load(repo_file("Oasis/scripts/monte_carlo.py"));

    const Config configs[] = {
        { 21, 3, "random", "random", 40 },
        { 15, 4, "first-legal", "random", 40 },
        { 30, 5, "random", "first-legal", 40 },
        { 7, 2, "monte-carlo", "random", 8 },
        { 10, 3, "monte-carlo", "first-legal", 8 },
        { 10, 3, "first-legal", "monte-carlo", 8 },
    };

    for (const Config& config : configs)
    {
        CAPTURE(config.stones);
        CAPTURE(config.max_take);
        CAPTURE(config.strategy_a);
        CAPTURE(config.strategy_b);

        Params params = nim_params(config.stones, config.max_take);
        BatchResult from_python = run_batch("nim", params, config.strategy_a, config.strategy_b, config.games);
        BatchResult from_cpp = run_batch(kCppNim, params, config.strategy_a, config.strategy_b, config.games);

        CHECK(from_python.matches == config.games);
        CHECK(from_python.decisions > 0);
        CHECK(from_python.matches == from_cpp.matches);
        CHECK(from_python.draws == from_cpp.draws);
        CHECK(from_python.decisions == from_cpp.decisions);
        REQUIRE(from_python.wins.size() == from_cpp.wins.size());
        for (size_t seat = 0; seat < from_python.wins.size(); ++seat)
        {
            CHECK(from_python.wins[seat] == from_cpp.wins[seat]);
        }
        check_same_outcome(Outcome{ true, from_python.rewards }, Outcome{ true, from_cpp.rewards });
    }
}

TEST_CASE("a Python Nim and its C++ port stay in lockstep through random play, apply and undo")
{
    ScopedCppNim cpp_nim;
    RunningPython python;
    python.load(repo_file("Oasis/scripts/nim.py"));

    for (int64_t max_take : { 2, 3, 5 })
    {
        UniquePtr<IGame> python_game = create_game("nim", nim_params(17, max_take));
        UniquePtr<IGame> cpp_game = create_game(kCppNim, nim_params(17, max_take));
        UniquePtr<IState> python_state = python_game->new_initial_state();
        UniquePtr<IState> cpp_state = cpp_game->new_initial_state();
        Random random(static_cast<uint64_t>(max_take));

        while (!cpp_state->is_terminal())
        {
            CHECK_FALSE(python_state->is_terminal());
            check_same_actions(python_state->legal_actions(), cpp_state->legal_actions());
            CHECK(python_state->current_player() == cpp_state->current_player());

            ActionList actions = cpp_state->legal_actions();
            ActionId action = actions[static_cast<size_t>(random.get_int(0, static_cast<int64_t>(actions.size()) - 1))];

            python_state->apply(action);
            cpp_state->apply(action);
            check_same_outcome(python_state->outcome(), cpp_state->outcome());

            if (!cpp_state->is_terminal())
            {
                ActionId probe = cpp_state->legal_actions()[0];
                python_state->apply(probe);
                cpp_state->apply(probe);
                python_state->undo(probe);
                cpp_state->undo(probe);
                check_same_actions(python_state->legal_actions(), cpp_state->legal_actions());
            }
        }

        CHECK(python_state->is_terminal());
        check_same_outcome(python_state->outcome(), cpp_state->outcome());
    }
}

#endif
