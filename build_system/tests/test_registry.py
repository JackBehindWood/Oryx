import typer

from build_system import registry

GROUPS = ["Build", "Config", "Docs", "Python", "Setup", "Test", "Vendor"]


def test_discovery_order():
    names = [module.__name__ for module in registry.discover_command_modules()]
    assert names == [f"build_system.commands.{group.lower()}" for group in GROUPS]


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
        "Config": ["init"],
        "Docs": ["build_docs", "serve_docs", "clean_docs"],
        "Python": ["generate_stubs"],
        "Setup": ["premake"],
        "Test": ["run_tests", "run_benchmarks"],
        "Vendor": ["add"],
    }


def test_only_compile_requires_vendor():
    registry.discover_command_modules()
    assert [entry.func.__name__ for entry in registry._REGISTRY if entry.requires_vendor] == ["compile_project"]


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


def test_vendor_check_skipped_under_dry_run(forge):
    assert forge("--dry-run", "build", "compile").exit_code == 0


def test_vendor_check_blocks_compile_before_its_body(forge):
    result = forge("build", "compile")
    assert result.exit_code == 1
    assert result.output.splitlines() == [
        "✗ Missing vendored submodule: Oryx/vendor/pybind11 is empty.",
        "✗ Missing vendored submodule: Oryx/vendor/spdlog is empty.",
        "✗ Missing vendored submodule: Oryx/vendor/yaml-cpp is empty.",
        "✗ Missing vendored submodule: tests/vendor/doctest is empty.",
        "  Run: git submodule update --init --recursive",
    ]


def test_vendor_check_ignores_pybind11_without_python(forge, tmp_project):
    for name in ("Oryx/vendor/spdlog", "Oryx/vendor/yaml-cpp", "tests/vendor/doctest"):
        (tmp_project / name / "README").touch()
    result = forge("--no-python", "--dry-run", "build", "compile")
    assert result.exit_code == 0
    assert "Missing vendored submodule" not in result.output
