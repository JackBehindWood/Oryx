import ast
import pathlib

import oryx


def test_the_type_stubs_declare_exactly_the_public_names_of_each_module():
    stubs_dir = pathlib.Path(__file__).resolve().parents[2] / "OryxPython" / "stubs" / "oryx"
    problems = []
    for path in sorted(stubs_dir.glob("*.pyi")):
        tree = ast.parse(path.read_text())
        if path.stem == "__init__":
            module = oryx
            declared = {
                alias.asname or alias.name
                for node in tree.body
                if isinstance(node, ast.ImportFrom) and node.module != "__future__"
                for alias in node.names
            }
            declared |= {node.name for node in tree.body if isinstance(node, (ast.ClassDef, ast.FunctionDef))}
        else:
            module = getattr(oryx, path.stem)
            declared = {node.name for node in tree.body if isinstance(node, (ast.ClassDef, ast.FunctionDef))}
            declared |= {node.target.id for node in tree.body if isinstance(node, ast.AnnAssign)}
        declared = {name for name in declared if not name.startswith("_")}
        actual = {name for name in dir(module) if not name.startswith("_")}
        if actual != declared:
            problems.append((path.name, sorted(actual - declared), sorted(declared - actual)))
    assert problems == []
