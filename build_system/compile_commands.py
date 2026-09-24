import platform
import shlex
import shutil
import subprocess
from pathlib import Path

from rich.console import Console

from .config import BuildConfig
from .utils import get_macos_sdk_path, run_command, write_json

console = Console()

COMPILE_COMMANDS_NAME = "compile_commands.json"


def _arg_after(tokens: list[str], flag: str) -> str | None:
    try:
        return tokens[tokens.index(flag) + 1]
    except (ValueError, IndexError):
        return None


def _compile_entries(make_file: Path, config_token: str) -> list[dict]:
    """Dry-run a generated gmake project file and pull out one compile_commands.json
    entry per translation unit from the fully-resolved compiler invocations `make`
    prints — real ground truth, not a re-implementation of Premake's flag logic."""
    # -k: a fresh build has no linked libs yet; make still prints every compile line before stopping there.
    try:
        output = run_command(
            ["make", "-n", "-B", "-k", "-f", make_file.name, f"config={config_token}"],
            cwd=make_file.parent,
        ).stdout
    except subprocess.CalledProcessError as error:
        output = error.stdout or ""
        if " -c " not in output:
            console.print(f"[yellow]⚠️ Skipping {make_file.name}: {error.stderr or error}[/yellow]")
            return []

    sdk = get_macos_sdk_path() if platform.system() == "Darwin" else None

    entries = []
    for line in output.splitlines():
        try:
            tokens = shlex.split(line)
        except ValueError:
            continue

        source = _arg_after(tokens, "-c")
        if not source:
            continue

        if sdk:
            # Real clang auto-detects the SDK for a bare `clang++` invocation
            # (that's why the build itself works without this), but tools
            # that just parse these arguments — VS Code's C/C++ extension,
            # clangd — don't replicate that and need it explicit.
            tokens = [tokens[0], "-isysroot", sdk] + tokens[1:]

        entries.append({
            "directory": str(make_file.parent),
            "file": source,
            "arguments": tokens,
            "output": _arg_after(tokens, "-o"),
        })

    return entries


def generate_compile_commands(cfg: BuildConfig, build_dir: Path) -> Path | None:
    """Generate compile_commands.json from the .make files Premake already wrote,
    covering every discovered project (Oryx.make, Oasis.make, Tests.make, and any
    future project) with its own real per-file includes/defines. Returns None (and
    prints a note) if `make` or the generated .make files aren't available yet —
    the caller should treat that as non-fatal."""
    if not shutil.which("make"):
        console.print("[yellow]⚠️ 'make' not found; skipping compile_commands.json generation.[/yellow]")
        return None

    make_files = sorted(build_dir.glob("*.make"))
    if not make_files:
        console.print("[yellow]⚠️ No generated .make files found; run configure first for compile_commands.json.[/yellow]")
        return None

    entries = []
    for make_file in make_files:
        entries.extend(_compile_entries(make_file, cfg.make_config_token))

    return write_json(build_dir / COMPILE_COMMANDS_NAME, entries)
