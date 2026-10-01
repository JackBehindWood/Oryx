import builtins

import pytest

from pyforge import interactive


@pytest.fixture
def no_questionary(monkeypatch):
    """Simulates `pyforge[menu]` not being installed: `import questionary` raises."""
    real_import = builtins.__import__

    def fake_import(name, *args, **kwargs):
        if name == "questionary":
            raise ImportError("no module named questionary")
        return real_import(name, *args, **kwargs)

    monkeypatch.setattr(builtins, "__import__", fake_import)


def test_questionary_or_none_returns_the_module_when_installed():
    assert interactive.questionary_or_none() is not None


def test_questionary_or_none_hints_at_the_extra_when_missing(no_questionary, capsys):
    assert interactive.questionary_or_none() is None
    assert 'pip install "pyforge[menu]"' in capsys.readouterr().err


def test_run_menu_falls_back_to_help_without_questionary(no_questionary, capsys):
    class FakeContext:
        def get_help(self):
            return "usage: forge ..."

    interactive.run_menu(FakeContext())
    assert "usage: forge ..." in capsys.readouterr().out
