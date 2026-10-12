import os
import re
import shutil
import subprocess
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path

from rich.console import Console

CHUNKS_PER_SHARD = 4
_COUNT = re.compile(r"passing the current filters:\s*(\d+)")
_SUMMARY = re.compile(r"test cases:\s*\d+\s*\|\s*\d+ passed\s*\|\s*(\d+) failed")


def source_filter(dirs: list[str]) -> str:
    return ",".join(f"*{d}/*" for d in dirs)


def command(binary: Path, dirs: list[str], extra: list[str], *, list_only: bool = False) -> list[str]:
    """The Tests binary, filtered to the given suite directories via --source-file."""
    flags = ["--list-test-cases"] if list_only else []
    return [str(binary), f"--source-file={source_filter(dirs)}", *flags, *extra]


def chunk_ranges(total: int, chunks: int) -> list[tuple[int, int]]:
    """Contiguous, 1-based, inclusive (first, last) test-case ranges covering `total` cases in at most `chunks` pieces."""
    chunks = max(1, min(chunks, total))
    size, extra = divmod(total, chunks)
    ranges, first = [], 1
    for index in range(chunks):
        last = first + size + (1 if index < extra else 0) - 1
        ranges.append((first, last))
        first = last + 1
    return ranges


def count_cases(binary: Path, dirs: list[str], extra: list[str], cwd: Path, env: dict) -> int | None:
    """How many test cases pass the filters, or None when the binary doesn't say."""
    result = subprocess.run(command(binary, dirs, extra) + ["--count"], cwd=cwd, env=env, capture_output=True, text=True)
    match = _COUNT.search(result.stdout)
    return int(match[1]) if match else None


@dataclass(frozen=True)
class ChunkResult:
    first: int
    last: int
    code: int
    seconds: float
    output: str

    @property
    def failed_cases(self) -> int:
        match = _SUMMARY.search(self.output)
        return int(match[1]) if match else (0 if self.code == 0 else 1)


def _run_chunk(argv: list[str], first: int, last: int, cwd: Path, env: dict) -> ChunkResult:
    scratch = tempfile.mkdtemp(prefix="forge-shard-")
    try:
        started = time.monotonic()
        result = subprocess.run(
            [*argv, f"--first={first}", f"--last={last}"],
            cwd=cwd,
            env={**env, "TMPDIR": scratch, "TMP": scratch, "TEMP": scratch},
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        return ChunkResult(first, last, result.returncode, time.monotonic() - started, result.stdout + result.stderr)
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def run_sharded(binary: Path, dirs: list[str], extra: list[str], *, shards: int, cwd: Path, env: dict, console: Console) -> int:
    """Run the filtered test cases as contiguous chunks over `shards` worker processes, each with its own TMPDIR.

    Returns 0 when every chunk passed. Falls back to one plain run when the case count can't be read.
    """
    shards = shards if shards > 0 else (os.cpu_count() or 1)
    total = count_cases(binary, dirs, extra, cwd, env)
    argv = command(binary, dirs, extra)
    if total is None or total < 2 or shards < 2:
        return subprocess.run(argv, cwd=cwd, env=env).returncode

    ranges = chunk_ranges(total, shards * CHUNKS_PER_SHARD)
    console.print(f"[dim]{total} test cases in {len(ranges)} chunks over {shards} processes[/dim]")
    started = time.monotonic()
    results: list[ChunkResult] = []
    with ThreadPoolExecutor(max_workers=shards) as pool:
        futures = [pool.submit(_run_chunk, argv, first, last, cwd, env) for first, last in ranges]
        for done in as_completed(futures):
            result = done.result()
            results.append(result)
            if result.code != 0:
                console.print(f"[bold red]✗ cases {result.first}-{result.last}[/bold red]")
                console.print(result.output, markup=False, highlight=False)
    failed = sum(result.failed_cases for result in results)
    elapsed = time.monotonic() - started
    if any(result.code != 0 for result in results):
        console.print(f"[bold red]{failed} of {total} test cases failed[/bold red] [dim]({elapsed:.1f}s)[/dim]")
        return 1
    console.print(f"[dim]{total} of {total} test cases passed in {elapsed:.1f}s[/dim]")
    return 0
