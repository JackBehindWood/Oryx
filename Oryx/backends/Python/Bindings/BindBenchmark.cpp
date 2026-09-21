#include "oxpch.h"
#include "Bindings/BindOryx.h"

#include "Support/PyBatch.h"
#include "Support/PyResolve.h"
#include "Oryx/Benchmark/MemoryBenchmarkRunner.h"
#include "Oryx/Benchmark/Timer.h"
#include "Oryx/Scripting/Support/InitGuard.h"

namespace py = pybind11;

namespace oryx::python
{

namespace
{

struct PyBenchmarkResult
{
    PyBatchResult outcome;
    double elapsed_seconds = 0.0;
    bool has_memory = false;
    MemoryStats memory;
};

template<typename Runner>
auto run_released(Runner& runner, int32_t games, bool holds_gil)
{
    if (holds_gil)
    {
        return runner.run(games);
    }
    py::gil_scoped_release release;
    return runner.run(games);
}

PyBenchmarkResult benchmark(const py::object& game, const py::sequence& strategies, int32_t games, const py::object& seed, bool memory)
{
    if (games < 0)
    {
        throw Error("the number of matches cannot be negative");
    }

    SharedPtr<IGame> resolved = resolve_game(game);
    Strategies participants = resolve_seeded_strategies(strategies, seed);
    std::vector<IStrategy*> raw = raw_pointers(participants);
    bool holds_gil = holds_gil_for(*resolved, raw);

    PyBenchmarkResult result;
    if (memory)
    {
        MemoryBenchmarkRunner runner(*resolved, to_small_vector(raw));
        MemoryBenchmarkRunner::MemoryResults results = run_released(runner, games, holds_gil);
        result.outcome.counts = results.outcome;
        result.elapsed_seconds = results.elapsed_seconds;
        result.has_memory = true;
        result.memory = results.memory;
    }
    else
    {
        BenchmarkRunner runner(*resolved, to_small_vector(raw));
        BenchmarkRunner::Results results = run_released(runner, games, holds_gil);
        result.outcome.counts = results.outcome;
        result.elapsed_seconds = results.elapsed_seconds;
    }
    result.outcome.has_metadata = true;
    result.outcome.metadata = make_metadata(*resolved, strategies, games, seed);
    return result;
}

BenchmarkRunner::Results as_results(const PyBenchmarkResult& result)
{
    return BenchmarkRunner::Results{ result.outcome.counts, result.elapsed_seconds };
}

} // namespace

void bind_benchmark(py::module_& module)
{
    py::module_ benchmarking = module.def_submodule("benchmark", "Timing and memory measurements of simulations.");

    py::class_<Timer>(benchmarking, "Timer", "Wall-clock timer; also a context manager. Read elapsed_seconds after stop().")
        .def(py::init<>())
        .def("start", &Timer::start)
        .def("stop", &Timer::stop)
        .def_property_readonly("elapsed_seconds", &Timer::elapsed_seconds)
        .def("__enter__", [](Timer& timer) -> Timer& { timer.start(); return timer; }, py::return_value_policy::reference_internal)
        .def("__exit__", [](Timer& timer, const py::args&) { timer.stop(); });

    py::class_<MemoryStats>(benchmarking, "MemoryStats", "C++ heap allocations made by Oryx during a run; Python's own allocator is not counted.")
        .def_readonly("allocation_count", &MemoryStats::allocation_count)
        .def_readonly("deallocation_count", &MemoryStats::deallocation_count)
        .def_readonly("bytes_allocated", &MemoryStats::bytes_allocated)
        .def_readonly("bytes_freed", &MemoryStats::bytes_freed)
        .def_readonly("live_bytes", &MemoryStats::live_bytes)
        .def_readonly("peak_live_bytes", &MemoryStats::peak_live_bytes)
        .def("__repr__", [](const MemoryStats& stats)
            {
                return "<oryx.benchmark.MemoryStats allocations=" + std::to_string(stats.allocation_count) + " bytes_allocated=" + std::to_string(stats.bytes_allocated) + " peak_live_bytes=" + std::to_string(stats.peak_live_bytes) + ">";
            });

    py::class_<PyBenchmarkResult>(benchmarking, "BenchmarkResult", "The outcome of a benchmarked batch with its wall-clock time and throughput.")
        .def_property_readonly("outcome", [](const PyBenchmarkResult& result) { return result.outcome; })
        .def_readonly("elapsed_seconds", &PyBenchmarkResult::elapsed_seconds)
        .def_property_readonly("matches_per_second", [](const PyBenchmarkResult& result) { return matches_per_second(as_results(result)); })
        .def_property_readonly("decisions_per_second", [](const PyBenchmarkResult& result) { return decisions_per_second(as_results(result)); })
        .def_property_readonly("memory", [](const PyBenchmarkResult& result) { return result.has_memory ? py::cast(result.memory) : py::none(); }, "MemoryStats when benchmark(..., memory=True), else None.")
        .def("__repr__", [](const PyBenchmarkResult& result)
            {
                return "<oryx.benchmark.BenchmarkResult matches=" + std::to_string(result.outcome.counts.matches) + " elapsed_seconds=" + std::to_string(result.elapsed_seconds) + " matches_per_second=" + std::to_string(matches_per_second(as_results(result))) + ">";
            });

    benchmarking.def("benchmark", OX_GUARDED_FUNC(benchmark, "oryx.benchmark.benchmark"),
                     py::arg("game"), py::arg("strategies"), py::arg("games") = 1000, py::arg("seed") = py::none(), py::arg("memory") = false,
                     "Plays `games` matches like simulate() and reports the time taken; memory=True also counts C++ allocations.");
}

} // namespace oryx::python
