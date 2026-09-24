import typer

from pyforge import registry

STATIC_GROUPS = ["Build", "Config", "Deps", "Docs", "Editor", "Init", "Premake", "Target", "Test"]
MODULES = STATIC_GROUPS
# "Python" (from build_system/oryx/, the Oryx plugin — see pyforge/main.py's plugin
# loading) mounts after every statically-discovered group, since it isn't one of the files
# pkgutil finds under pyforge/commands/.
GROUPS = [*STATIC_GROUPS, "Python"]


def test_discovery_order():
    names = [module.__name__ for module in registry.discover_command_modules()]
    assert names == [f"pyforge.commands.{group.lower()}" for group in MODULES]


def test_discovery_is_idempotent():
    registry.discover_command_modules()
    count = len(registry._REGISTRY)
    registry.discover_command_modules()
    assert len(registry._REGISTRY) == count


def test_groups_in_order():
    assert registry.groups_in_order() == GROUPS


def test_entries_for_group():
    labels = {group: [entry.func.__name__ for entry in registry.entries_for_group(group)] for group in GROUPS}
    assert labels == {
        "Build": ["configure", "compile_project", "clean", "run_all", "run_project"],
        "Config": ["show", "get", "set_", "unset_"],
        "Deps": ["add", "sync", "update", "status", "remove"],
        "Docs": ["build_docs", "serve_docs", "clean_docs"],
        "Editor": ["vscode", "vs2022"],
        "Init": ["init"],
        "Premake": ["status", "install", "update"],
        "Python": ["generate_stubs"],
        "Target": ["add", "remove", "list_"],
        "Test": ["run_suites"],
    }


def test_only_compile_requires_dependencies():
    registry.discover_command_modules()
    assert [entry.func.__name__ for entry in registry._REGISTRY if entry.requires_dependencies] == ["compile_project"]


def test_new_group_is_listed_after_discovered_groups(monkeypatch):
    monkeypatch.setattr(registry, "_REGISTRY", list(registry._REGISTRY))
    app = typer.Typer()
    command = registry.make_group(app, group="Extra")

    @command(name="visible", label="Visible")
    def visible(ctx: typer.Context):
        pass

    @command(name="internal", label="Internal", hidden=True)
    def internal(ctx: typer.Context):
        pass

    assert [c.name for c in app.registered_commands] == ["visible", "internal"]
    assert [entry.label for entry in registry.entries_for_group("Extra")] == ["Visible"]
    assert registry.groups_in_order() == [*GROUPS, "Extra"]


def test_hidden_only_group_is_not_listed(monkeypatch):
    monkeypatch.setattr(registry, "_REGISTRY", list(registry._REGISTRY))
    command = registry.make_group(typer.Typer(), group="Plumbing")

    @command(name="internal", label="Internal", hidden=True)
    def internal(ctx: typer.Context):
        pass

    assert "Plumbing" not in registry.groups_in_order()


def test_dependency_check_skipped_under_dry_run(forge):
    assert forge("--dry-run", "compile").exit_code == 0


def test_dependency_check_blocks_compile_before_its_body(forge, tmp_project):
    (tmp_project / "forge.toml").write_text(
        (tmp_project / "forge.toml").read_text(encoding="utf-8").replace('fetch = "never"', 'fetch = "never"'), encoding="utf-8"
    )
    result = forge("compile")
    assert result.exit_code == 1
    assert "Missing dependencies: spdlog (Oryx/vendor/spdlog), yaml-cpp (Oryx/vendor/yaml-cpp), pybind11 (Oryx/vendor/pybind11)" in result.output
