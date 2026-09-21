#include "doctest.h"

#include "unit/Python/PythonTestSupport.h"

using namespace oryx;
using namespace oryx::test;

#ifdef OX_ENABLE_PYTHON

namespace
{

class WorkingDirectory
{
public:
    explicit WorkingDirectory(const std::filesystem::path& directory)
        : m_previous(std::filesystem::current_path())
    {
        std::filesystem::current_path(directory);
    }

    ~WorkingDirectory() { std::filesystem::current_path(m_previous); }

    WorkingDirectory(const WorkingDirectory&) = delete;
    WorkingDirectory& operator=(const WorkingDirectory&) = delete;

private:
    std::filesystem::path m_previous;
};

std::string game_script(const std::string& id_expression)
{
    return "import oryx\n"
           "class LayerGame(oryx.Game, id=" + id_expression + "):\n"
           "    num_players = 2\n"
           "    def new_initial_state(self): return None\n";
}

void push_python_layer(LayerStack& stack, const TempDir& dir)
{
    ScriptDiscoveryOptions options;
    options.root = dir.path();

    std::vector<UniquePtr<IScriptRuntime>> runtimes;
    runtimes.push_back(python_runtime());
    stack.push_layer<ScriptingLayer>(options, std::move(runtimes));
}

} // namespace

TEST_CASE("ScriptingLayer finds a script nested anywhere under the root with no configuration")
{
    TempDir dir;
    dir.write("games/deep/layer-nested.oryx.py", game_script("'layer-nested'"));

    {
        LayerStack stack;
        push_python_layer(stack, dir);
        CHECK(GameRegistry::has("layer-nested"));
    }
    CHECK_FALSE(GameRegistry::has("layer-nested"));
}

TEST_CASE("a script can import a helper module that sits in the project root")
{
    TempDir dir;
    dir.write("layer_helpers.py", "GAME_ID = 'layer-helper'\n");
    dir.write("scripts/uses.oryx.py", "import layer_helpers\n" + game_script("layer_helpers.GAME_ID"));

    WorkingDirectory root(dir.path());
    LayerStack stack;
    push_python_layer(stack, dir);

    CHECK(GameRegistry::has("layer-helper"));
}

TEST_CASE("reloading replaces what a script defines and drops what it no longer defines")
{
    TempDir dir;
    std::filesystem::path script = dir.write("swap.oryx.py", game_script("'layer-before'"));

    LayerStack stack;
    push_python_layer(stack, dir);
    REQUIRE(GameRegistry::has("layer-before"));

    dir.write("swap.oryx.py", game_script("'layer-after'"));
    ReloadScriptsEvent event;
    stack.dispatch_event(event);

    CHECK(GameRegistry::has("layer-after"));
    CHECK_FALSE(GameRegistry::has("layer-before"));

    std::filesystem::remove(script);
    stack.dispatch_event(event);
    CHECK_FALSE(GameRegistry::has("layer-after"));
}

TEST_CASE("reloading picks up a script added after startup")
{
    TempDir dir;
    dir.write("first.oryx.py", game_script("'layer-first'"));

    LayerStack stack;
    push_python_layer(stack, dir);

    dir.write("late/second.oryx.py", game_script("'layer-second'"));
    ReloadScriptsEvent event;
    stack.dispatch_event(event);

    CHECK(GameRegistry::has("layer-first"));
    CHECK(GameRegistry::has("layer-second"));
}

TEST_CASE("a game created from a script can outlive the interpreter and be destroyed afterwards")
{
    TempDir dir;
    dir.write("outlive.oryx.py", game_script("'layer-outlive'"));

    UniquePtr<IGame> game;
    {
        LayerStack stack;
        push_python_layer(stack, dir);
        game = create_game("layer-outlive");
        REQUIRE(game != nullptr);
    }

    CHECK_FALSE(GameRegistry::has("layer-outlive"));
    CHECK_NOTHROW(game.reset());
}

TEST_CASE("the embedded interpreter sees the venv's site-packages")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write("paths.oryx.py", marker_prelude(marker) +
        "import sys\nmark('|'.join(p for p in sys.path if p.endswith('site-packages')))\n");

    RunningPython python;
    python.load(script);

    CHECK(read_file(marker).find("site-packages") != std::string::npos);
}

#endif
