import time

import oryx
import pytest


def test_timer_measures_a_with_block():
    with oryx.benchmark.Timer() as timer:
        time.sleep(0.01)
    assert timer.elapsed_seconds >= 0.01

    manual = oryx.benchmark.Timer()
    manual.start()
    manual.stop()
    assert 0.0 <= manual.elapsed_seconds < 1.0


def test_benchmark_reports_the_same_outcome_as_simulate_with_timing_and_throughput():
    b = oryx.benchmark.benchmark("nim", ["random", "random"], games=30, seed=1)
    s = oryx.simulate("nim", ["random", "random"], games=30, seed=1)
    assert b.outcome.wins == s.wins
    assert b.outcome.draws == s.draws
    assert b.outcome.decisions == s.decisions
    assert b.elapsed_seconds > 0
    assert abs(b.matches_per_second * b.elapsed_seconds - 30) < 1e-6
    assert b.decisions_per_second > 0
    assert b.memory is None
    assert b.outcome.metadata["seed"] == 1
    assert repr(b).startswith("<oryx.benchmark.BenchmarkResult")


def test_benchmark_counts_allocations_only_when_asked_and_rejects_a_negative_count():
    m = oryx.benchmark.benchmark("nim", ["random", "random"], games=10, seed=1, memory=True).memory
    assert isinstance(m, oryx.benchmark.MemoryStats)
    assert m.allocation_count >= 0
    assert m.peak_live_bytes >= 0

    with pytest.raises(oryx.OryxError, match="the number of matches cannot be negative"):
        oryx.benchmark.benchmark("nim", ["random", "random"], games=-1)
