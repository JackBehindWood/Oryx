import tomllib

import pytest

from pyforge import options
from pyforge.config import LocalConfig, OptionSpec, SchemaError, parse_config

CFG = parse_config(tomllib.loads(
    '[project]\nname = "x"\n[options]\n'
    'python = { default = true, off = "--no-python" }\n'
    'sanitize = { default = false, on = "--sanitize" }\n'
    'gui = { default = false, on = "--gui", off = "--headless" }\n'
    'silent = { default = true }\n'
))


def test_defaults():
    assert options.resolve(CFG, LocalConfig()) == {"python": True, "sanitize": False, "gui": False, "silent": True}


def test_local_then_cli_override():
    local = LocalConfig(options={"python": False, "gui": True})
    assert options.resolve(CFG, local, with_=["python"], without=["gui"]) == {"python": True, "sanitize": False, "gui": False, "silent": True}


def test_unknown_option_suggests():
    with pytest.raises(SchemaError, match="--without pyton: unknown option — did you mean 'python'"):
        options.resolve(CFG, LocalConfig(), without=["pyton"])


@pytest.mark.parametrize(
    ("values", "flags"),
    [
        ({"python": True, "sanitize": False, "gui": False, "silent": True}, ["--headless"]),
        ({"python": False, "sanitize": True, "gui": True, "silent": False}, ["--no-python", "--sanitize", "--gui"]),
    ],
)
def test_flags_follow_declaration_order(values, flags):
    assert options.premake_flags(CFG.options, values) == flags


def test_defines_come_last():
    values = {"python": False, "sanitize": False, "gui": False, "silent": True}
    assert options.premake_flags(CFG.options, values, ["cc=clang", "fast", "path=a b"]) == ["--no-python", "--headless", "--cc=clang", "--fast", "--path=a b"]


@pytest.mark.parametrize("define", ["", "=x", "-x", "a b"])
def test_bad_defines(define):
    with pytest.raises(SchemaError, match="expected KEY or KEY=VALUE"):
        options.define_flag(define)


def test_hash_is_order_sensitive_and_stable():
    assert options.options_hash(["a", "b"]) == options.options_hash(["a", "b"])
    assert options.options_hash(["a", "b"]) != options.options_hash(["b", "a"])
    assert options.options_hash(["ab"]) != options.options_hash(["a", "b"])


def test_spec_without_flags_produces_none():
    assert options.premake_flags({"x": OptionSpec()}, {"x": True}) == []
