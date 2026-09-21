#include "doctest.h"

#include "unit/Python/PythonTestSupport.h"

#ifdef OX_ENABLE_PYTHON

#include <chrono>

using namespace oryx;
using namespace oryx::test;

namespace
{

constexpr int64_t kCalls = 200'000;
constexpr int32_t kDecisions = 40;
constexpr int64_t kPlayoutsPerDecision = 90;

constexpr const char* kFirstLegalStrategy = R"(
import oryx

class BenchFirst(oryx.Strategy, id="bench-first"):
    def decide(self, context):
        return context.state.legal_actions()[0]
)";

using Clock = std::chrono::steady_clock;

double nanoseconds_since(Clock::time_point start)
{
    return std::chrono::duration<double, std::nano>(Clock::now() - start).count();
}

template<typename Fn>
double ns_per_call(int64_t calls, Fn&& fn)
{
    Clock::time_point start = Clock::now();
    for (int64_t i = 0; i < calls; ++i)
    {
        fn();
    }
    return nanoseconds_since(start) / static_cast<double>(calls);
}

void print_ns(const char* label, double ns)
{
    std::cout << "  " << label << ": " << static_cast<int64_t>(ns) << " ns/call\n";
}

void print_batch(const char* label, const BenchmarkRunner::Results& results)
{
    std::cout << "  " << label << ": " << results.outcome.matches << " matches, " << results.outcome.decisions << " decisions in "
              << results.elapsed_seconds << " s (" << static_cast<int64_t>(matches_per_second(results)) << " matches/s, "
              << static_cast<int64_t>(decisions_per_second(results)) << " decisions/s)\n";
}

BenchmarkRunner::Results run_benchmark(const IGame& game, const char* strategy_a, const char* strategy_b, int32_t games)
{
    UniquePtr<IStrategy> a = StrategyRegistry::create(strategy_a, {});
    UniquePtr<IStrategy> b = StrategyRegistry::create(strategy_b, {});
    return BenchmarkRunner(game, { a.get(), b.get() }).run(games);
}

} // namespace

TEST_SUITE("benchmark")
{

TEST_CASE("Benchmark: Python adapter calls on a Python Nim state")
{
    RunningPython python;
    python.load(repo_file("Oasis/scripts/nim.py"));

    UniquePtr<IGame> game = create_game("nim", Params{ { "stones", kCalls + 10 }, { "max_take", int64_t{ 3 } } });
    UniquePtr<IState> state = game->new_initial_state();
    volatile size_t sink = 0;

    std::cout << "\nPython Nim state adapter (" << kCalls << " calls each)\n";
    print_ns("legal_actions", ns_per_call(kCalls, [&] { sink += state->legal_actions().size(); }));
    print_ns("current_player", ns_per_call(kCalls, [&] { sink += static_cast<size_t>(state->current_player()); }));
    print_ns("is_terminal", ns_per_call(kCalls, [&] { sink += state->is_terminal() ? 1 : 0; }));
    print_ns("outcome", ns_per_call(kCalls, [&] { sink += state->outcome().rewards.player_count(); }));
    print_ns("action_to_string", ns_per_call(kCalls, [&] { sink += state->action_to_string(1).size(); }));
    print_ns("apply", ns_per_call(kCalls, [&] { state->apply(1); }));
    print_ns("undo", ns_per_call(kCalls, [&] { state->undo(1); }));
    print_ns("apply+undo pair", ns_per_call(kCalls, [&] { state->apply(1); state->undo(1); }));

    CHECK(state->legal_actions().size() == 3);
}

TEST_CASE("Benchmark: Python strategy decide() overhead")
{
    TempDir dir;
    RunningPython python;
    python.load(repo_file("Oasis/scripts/nim.py"));
    python.load(dir.write("bench_first.py", kFirstLegalStrategy));

    UniquePtr<IGame> game = create_game("nim", Params{ { "stones", int64_t{ 21 } }, { "max_take", int64_t{ 3 } } });
    UniquePtr<IState> state = game->new_initial_state();
    Context context(*state);
    UniquePtr<IStrategy> strategy = StrategyRegistry::create("bench-first", {});
    volatile size_t sink = 0;

    std::cout << "\nPython strategy decide() (" << kCalls / 4 << " calls)\n";
    print_ns("decide (first legal action)", ns_per_call(kCalls / 4, [&] { sink += strategy->decide(context); }));
}

TEST_CASE("Benchmark: Python Monte Carlo strategy")
{
    RunningPython python;
    python.load(repo_file("Oasis/scripts/nim.py"));
    python.load(repo_file("Oasis/scripts/monte_carlo.py"));

    UniquePtr<IGame> game = create_game("nim", Params{ { "stones", int64_t{ 21 } }, { "max_take", int64_t{ 3 } } });
    UniquePtr<IState> state = game->new_initial_state();
    Context context(*state);
    UniquePtr<IStrategy> strategy = StrategyRegistry::create("monte-carlo", {});

    Clock::time_point start = Clock::now();
    ActionId action = 0;
    for (int32_t i = 0; i < kDecisions; ++i)
    {
        action = strategy->decide(context);
    }
    double seconds = nanoseconds_since(start) / 1e9;

    std::cout << "\nPython Monte Carlo on Python Nim (21 stones, 30 playouts x 3 actions per decision)\n";
    std::cout << "  " << static_cast<double>(kDecisions) / seconds << " decisions/s, "
              << static_cast<double>(kDecisions * kPlayoutsPerDecision) / seconds << " playouts/s\n";
    CHECK(is_valid(action));
}

TEST_CASE("Benchmark: end-to-end batches with Python participants")
{
    RunningPython python;
    python.load(repo_file("Oasis/scripts/nim.py"));
    python.load(repo_file("Oasis/scripts/monte_carlo.py"));

    UniquePtr<IGame> nim = create_game("nim");
    UniquePtr<IGame> tictactoe = create_game("tictactoe");

    std::cout << "\nEnd-to-end (BenchmarkRunner)\n";
    print_batch("Python Nim, first-legal vs random x2000", run_benchmark(*nim, "first-legal", "random", 2000));
    print_batch("Python Monte Carlo vs first-legal, C++ TicTacToe x20", run_benchmark(*tictactoe, "monte-carlo", "first-legal", 20));
    print_batch("Python Monte Carlo vs first-legal, Python Nim x10", run_benchmark(*nim, "monte-carlo", "first-legal", 10));
}

}

#endif
