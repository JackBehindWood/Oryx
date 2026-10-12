import pytest

from pyforge.runners.doctest import ChunkResult, chunk_ranges


@pytest.mark.parametrize("total, chunks", [(10, 3), (7, 7), (5, 20), (1735, 32), (2, 1)])
def test_chunks_cover_every_case_exactly_once(total, chunks):
    ranges = chunk_ranges(total, chunks)
    covered = [number for first, last in ranges for number in range(first, last + 1)]
    assert covered == list(range(1, total + 1))
    assert len(ranges) == min(chunks, total)
    assert max(last - first for first, last in ranges) - min(last - first for first, last in ranges) <= 1


def test_failed_cases_come_from_the_doctest_summary():
    output = "[doctest] test cases: 41 | 39 passed | 2 failed | 1709 skipped\n"
    assert ChunkResult(1, 41, 1, 0.1, output).failed_cases == 2


def test_a_crash_without_a_summary_counts_as_one_failure():
    assert ChunkResult(1, 4, -11, 0.1, "Segmentation fault").failed_cases == 1
    assert ChunkResult(1, 4, 0, 0.1, "").failed_cases == 0
