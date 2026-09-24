import subprocess
from collections import deque
from pathlib import Path

from rich.console import Console

RING_BUFFER_LINES = 200


def stream_command(command: list[str], cwd: Path | None = None, env: dict | None = None, console: Console | None = None) -> int:
    """Run `command`, printing its combined stdout/stderr live, and return its exit code.

    Output is never held in full (unlike run_command's capture_output): a
    bounded ring buffer keeps only the last RING_BUFFER_LINES, re-printed as a
    failure recap for logs (e.g. CI) where the live stream already scrolled by.
    """
    console = console or Console()
    ring: deque[str] = deque(maxlen=RING_BUFFER_LINES)
    process = subprocess.Popen(
        command,
        cwd=str(cwd) if cwd else None,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )
    assert process.stdout is not None
    for line in process.stdout:
        console.print(line, end="", markup=False, highlight=False)
        ring.append(line)
    code = process.wait()
    if code != 0 and ring:
        console.print(f"\n[dim]— last {len(ring)} lines —[/dim]")
        for line in ring:
            console.print(line, end="", markup=False, highlight=False)
    return code
