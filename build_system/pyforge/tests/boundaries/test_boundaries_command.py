import json
from types import SimpleNamespace

import pytest
import typer

from pyforge.boundaries import BoundarySpec, BoundarySuite, Violation
from pyforge.commands import boundaries as command


def _run(*results, table=None):
    hook = SimpleNamespace(forge_boundaries=lambda ctx: list(results))
    return SimpleNamespace(pm=SimpleNamespace(hook=hook), config=SimpleNamespace(boundaries=table))


def test_a_plugin_may_return_a_spec_a_suite_or_nothing():
    suite = BoundarySuite()
    suite.new("python", root="tools")
    single = BoundarySpec(scan="cpp")
    run = _run(single, suite, None)
    assert [spec.name for spec in command._specs(run)] == ["cpp", "python"]


def test_two_specs_with_the_same_name_are_an_error():
    run = _run(BoundarySpec(scan="cpp"), BoundarySpec(scan="cpp"))
    with pytest.raises(typer.Exit):
        command._specs(run)


def test_json_output_carries_rule_file_line_and_spec(capsys):
    problem = Violation("A/x.h", "A must not include B", "Oryx/A/x.h", 3, "A !-> B", "cpp")
    waived = Violation("C/x.h", "C must not include D (waived)", "Oryx/C/x.h", 1, "C !-> D", "cpp")
    command._emit([problem], [waived], "json", several=False)
    rows = json.loads(capsys.readouterr().out)
    assert rows == [
        {"spec": "cpp", "rule": "A !-> B", "file": "Oryx/A/x.h", "line": 3, "message": "A must not include B", "waived": False},
        {"spec": "cpp", "rule": "C !-> D", "file": "Oryx/C/x.h", "line": 1, "message": "C must not include D (waived)", "waived": True},
    ]


def test_github_output_emits_an_error_annotation(capsys):
    command._emit([Violation("A/x.h", "msg", "Oryx/A/x.h", 3, "r", "cpp")], [], "github", several=True)
    assert "::error file=Oryx/A/x.h,line=3,title=Boundary violation::msg" in capsys.readouterr().out
