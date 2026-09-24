from typing import Optional

import typer
from rich.console import Console
from rich.markup import escape

from pyforge import registry, tomledit, workspace
from pyforge.config import RunContext, parse_config

console = Console()
GROUP_HELP = "Create a forge.toml for this project"

_APP_LUA = '''require "forge"

workspace "{name}"
    configurations {{ "Debug", "Release", "Dist" }}
    location "build"

    filter "configurations:Debug"
        symbols "On"
        optimize "Off"
    filter "configurations:Release"
        optimize "On"
    filter "configurations:Dist"
        optimize "On"
    filter {{}}

outputdir = "%{{cfg.buildcfg}}-%{{cfg.system}}-%{{cfg.architecture}}"

project "{name}"
    kind "ConsoleApp"
    forge.project_defaults()
    files {{ "src/**.cpp", "src/**.h" }}
'''

_APP_MAIN_CPP = """#include <cstdio>

int main() {{
    std::printf("Hello from {name}!\\n");
    return 0;
}}
"""


def _fail(message: str) -> typer.Exit:
    console.print(f"[bold red]✗ {message}[/bold red]")
    return typer.Exit(code=1)


def _project_name(run: RunContext, yes: bool) -> str:
    default = run.project.root.name
    from pyforge.interactive import is_interactive

    if yes or not is_interactive():
        return default
    import questionary

    return questionary.text("Project name", default=default).ask() or default


def _scaffold_app(run: RunContext, name: str) -> None:
    (run.project.root / "premake5.lua").write_text(_APP_LUA.format(name=name), encoding="utf-8")
    src = run.project.root / "src"
    src.mkdir(exist_ok=True)
    (src / "main.cpp").write_text(_APP_MAIN_CPP.format(name=name), encoding="utf-8")


def _derive_targets(ctx: typer.Context, run: RunContext) -> dict[str, str]:
    """Best-effort: run a real Premake configure and read [targets] entries off its export."""
    from pyforge.commands.build import EXECUTABLE_KINDS, configure

    try:
        ctx.invoke(configure, ctx)
    except typer.Exit:
        console.print("[yellow]Note: couldn't run Premake to detect [targets]; add them to forge.toml yourself.[/yellow]")
        return {}
    ws = workspace.load(run.project)
    if ws is None:
        return {}
    return {name.lower(): name for name, project in ws.projects.items() if any(cfg.kind in EXECUTABLE_KINDS for cfg in project.configs)}


def init(
    ctx: typer.Context,
    yes: bool = typer.Option(False, "--yes", help="Take detected defaults without asking."),
    template: Optional[str] = typer.Option(
        None, "--template", help="Scaffold a new project when no premake5.lua exists (currently: 'app', a minimal C++ console app)."
    ),
):
    """Create forge.toml: deriving [targets] from an existing premake5.lua's Premake export, or scaffolding
    a new project with --template. Editor files: `forge editor vscode|vs2022`."""
    run: RunContext = ctx.obj
    config_file = run.project.config_file
    if config_file.exists():
        console.print(f"⚠️ Configuration file already exists at: {config_file}")
        return
    if template not in (None, "app"):
        raise _fail(f"Unknown --template '{template}' (available: app)")

    if run.dry_run:
        console.print(f"[dim][dry-run] would create: {config_file}[/dim]")
        return

    name = _project_name(run, yes)
    config_file.write_text(tomledit.dumps({"project": {"name": name}}), encoding="utf-8")
    console.print(f"✓ Created default build configuration at: {config_file}")

    has_premake = (run.project.root / "premake5.lua").is_file()
    if not has_premake and template == "app":
        _scaffold_app(run, name)
        console.print(f"✓ Scaffolded a minimal C++ app in {run.project.root / 'src'}")
        has_premake = True
    elif not has_premake:
        console.print("[dim]No premake5.lua found; run 'forge init --template app' to scaffold one, or write your own and re-run 'forge init'.[/dim]")
        return

    targets = _derive_targets(ctx, run)
    if not targets:
        return

    def edit(text: str) -> str:
        for target_name, project_name in targets.items():
            text = tomledit.set_value(text, ["targets", target_name, "project"], project_name)
        return tomledit.set_value(text, ["project", "default-target"], next(iter(targets)))

    tomledit.edit_file(config_file, edit, validate=parse_config)
    console.print(f"[bold green]✓ forge.toml[/bold green] {escape('[targets]')}: {', '.join(targets)}")


ROOT_COMMAND = init
registry.register_entry(group="Init", label="Init — create a forge.toml", func=init)
