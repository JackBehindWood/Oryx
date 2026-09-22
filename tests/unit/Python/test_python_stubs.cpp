#include "doctest.h"

#include "unit/Python/PythonTestSupport.h"

using namespace oryx;
using namespace oryx::test;

#ifdef OX_ENABLE_PYTHON

TEST_CASE("the type stubs declare exactly the public names of each module")
{
    std::string stubs = repo_file("Oryx/backends/Python/stubs/oryx").generic_string();

    std::string output = run_oryx_script(
        "import ast, pathlib\n"
        "problems = []\n"
        "for path in sorted(pathlib.Path(r'" + stubs + "').glob('*.pyi')):\n"
        "    tree = ast.parse(path.read_text())\n"
        "    if path.stem == '__init__':\n"
        "        module = oryx\n"
        "        declared = {a.asname or a.name for node in tree.body if isinstance(node, ast.ImportFrom) and node.module != '__future__' for a in node.names}\n"
        "        declared |= {node.name for node in tree.body if isinstance(node, (ast.ClassDef, ast.FunctionDef))}\n"
        "    else:\n"
        "        module = getattr(oryx, path.stem)\n"
        "        declared = {node.name for node in tree.body if isinstance(node, (ast.ClassDef, ast.FunctionDef))}\n"
        "        declared |= {node.target.id for node in tree.body if isinstance(node, ast.AnnAssign)}\n"
        "    declared = {name for name in declared if not name.startswith('_')}\n"
        "    actual = {name for name in dir(module) if not name.startswith('_')}\n"
        "    if actual != declared:\n"
        "        problems.append((path.name, 'missing stub: ' + str(sorted(actual - declared)), 'no such name: ' + str(sorted(declared - actual))))\n"
        "mark(str(problems))\n");

    CHECK(output == "[]");
}

#endif
