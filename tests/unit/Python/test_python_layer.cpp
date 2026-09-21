#include "doctest.h"

#include "unit/Python/PythonTestSupport.h"
#include "unit/TestLogCapture.h"

using namespace oryx;
using namespace oryx::test;

#ifdef OX_ENABLE_PYTHON

namespace
{

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
    options.roots = { dir.path().string() };

    stack.push_layer<ScriptingLayer>(options, ScriptRuntimeRegistry::runtimes());
}

} // namespace

TEST_CASE("ScriptingLayer finds a script nested anywhere under the root with no configuration")
{
    TempDir dir;
    dir.write("games/deep/layer-nested.py", game_script("'layer-nested'"));
    ScopedScriptingShutdown shutdown_scope;

    {
        LayerStack stack;
        push_python_layer(stack, dir);
        CHECK(GameRegistry::has("layer-nested"));
    }
    CHECK(GameRegistry::has("layer-nested"));

    shutdown();
    CHECK_FALSE(GameRegistry::has("layer-nested"));
}

TEST_CASE("a script can import an underscore helper that sits in its root")
{
    TempDir dir;
    dir.write("_layer_helpers.py", "GAME_ID = 'layer-helper'\n");
    dir.write("scripts/uses.py", "import _layer_helpers\n" + game_script("_layer_helpers.GAME_ID"));
    ScopedScriptingShutdown shutdown_scope;

    LayerStack stack;
    push_python_layer(stack, dir);

    CHECK(GameRegistry::has("layer-helper"));
}

TEST_CASE("reloading replaces what a script defines and drops what it no longer defines")
{
    TempDir dir;
    std::filesystem::path script = dir.write("swap.py", game_script("'layer-before'"));
    ScopedScriptingShutdown shutdown_scope;

    LayerStack stack;
    push_python_layer(stack, dir);
    REQUIRE(GameRegistry::has("layer-before"));

    dir.write("swap.py", game_script("'layer-after'"));
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
    dir.write("first.py", game_script("'layer-first'"));
    ScopedScriptingShutdown shutdown_scope;

    LayerStack stack;
    push_python_layer(stack, dir);

    dir.write("late/second.py", game_script("'layer-second'"));
    ReloadScriptsEvent event;
    stack.dispatch_event(event);

    CHECK(GameRegistry::has("layer-first"));
    CHECK(GameRegistry::has("layer-second"));
}

TEST_CASE("reloading keeps what a helper module registers, because helpers are imported again")
{
    TempDir dir;
    dir.write("_shared.py",
        "import oryx\n"
        "class Shared(oryx.Game, id='layer-shared'):\n"
        "    num_players = 2\n"
        "    def new_initial_state(self): return None\n");
    dir.write("uses.py", "import _shared\n" + game_script("'layer-uses'"));
    ScopedScriptingShutdown shutdown_scope;

    LayerStack stack;
    push_python_layer(stack, dir);
    REQUIRE(GameRegistry::has("layer-shared"));
    REQUIRE(GameRegistry::has("layer-uses"));

    ReloadScriptsEvent event;
    stack.dispatch_event(event);

    CHECK(GameRegistry::has("layer-shared"));
    CHECK(GameRegistry::has("layer-uses"));
}

TEST_CASE("a broken edit is logged on reload, leaves the layer running, and the other scripts still load")
{
    TempDir dir;
    dir.write("good.py", game_script("'layer-good'"));
    std::filesystem::path fragile = dir.write("fragile.py", game_script("'layer-fragile'"));
    ScopedScriptingShutdown shutdown_scope;
    CoreLogCapture log;

    LayerStack stack;
    ScriptingLayer& layer = stack.push_layer<ScriptingLayer>(ScriptDiscoveryOptions{ {}, {}, { dir.path().string() }, dir.path() }, ScriptRuntimeRegistry::runtimes());
    REQUIRE(GameRegistry::has("layer-fragile"));

    dir.write("fragile.py", "import oryx\nraise RuntimeError('typo')\n");
    ReloadScriptsEvent event;
    stack.dispatch_event(event);

    CHECK_FALSE(layer.is_disabled());
    CHECK(GameRegistry::has("layer-good"));
    CHECK_FALSE(GameRegistry::has("layer-fragile"));

    std::vector<std::string> lines = log.lines();
    CHECK(std::any_of(lines.begin(), lines.end(), [](const std::string& line) { return line.find("typo") != std::string::npos; }));

    dir.write("fragile.py", game_script("'layer-fragile'"));
    stack.dispatch_event(event);
    CHECK(GameRegistry::has("layer-fragile"));
}

TEST_CASE("scripts in nested directories are imported by their dotted name")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    dir.write("deeper/alpha.py", marker_prelude(marker) + "mark(__name__)\n");
    ScopedScriptingShutdown shutdown_scope;

    LayerStack stack;
    push_python_layer(stack, dir);

    CHECK(read_file(marker) == "deeper.alpha");
}

TEST_CASE("the scripting roots come from the settings file, and a reload picks up an edited list of roots")
{
    TempDir dir;
    dir.write("first/one.py", game_script("'layer-one'"));
    std::filesystem::path settings = dir.write("oryx.yaml", "scripting:\n  roots: [first]\n");
    ScopedScriptingShutdown shutdown_scope;

    std::string flag = "--settings=" + settings.string();
    std::vector<std::string> storage = { "app", flag };
    std::vector<char*> pointers = { storage[0].data(), storage[1].data() };
    load_settings(ApplicationCommandLineArgs{ 2, pointers.data() });

    LayerStack stack;
    stack.push_layer<ScriptingLayer>(ScriptDiscoveryOptions{ {}, {}, {}, dir.path() }, ScriptRuntimeRegistry::runtimes());
    CHECK(GameRegistry::has("layer-one"));

    dir.write("second/two.py", game_script("'layer-two'"));
    dir.write("oryx.yaml", "scripting:\n  roots: [first, second]\n");
    ReloadScriptsEvent event;
    stack.dispatch_event(event);
    CHECK(GameRegistry::has("layer-one"));
    CHECK(GameRegistry::has("layer-two"));

    dir.write("oryx.yaml", "scripting:\n  roots: [first\n");
    stack.dispatch_event(event);
    CHECK(GameRegistry::has("layer-one"));
    CHECK(GameRegistry::has("layer-two"));
    CHECK(settings_of<ScriptSettings>().roots.size() == 2);

    reset_settings();
}

TEST_CASE("a game created from a script is freed cleanly when it is destroyed before shutdown")
{
    TempDir dir;
    dir.write("outlive.py", game_script("'layer-outlive'"));
    ScopedScriptingShutdown shutdown_scope;
    CoreLogCapture log;

    UniquePtr<IGame> game;
    {
        LayerStack stack;
        push_python_layer(stack, dir);
        game = create_game("layer-outlive");
        REQUIRE(game != nullptr);
    }

    CHECK(GameRegistry::has("layer-outlive"));
    CHECK(python::live_script_objects() > 0);
    game.reset();
    shutdown();

    CHECK_FALSE(GameRegistry::has("layer-outlive"));
    CHECK(python::live_script_objects() == 0);
    for (const std::string& line : log.lines())
    {
        CHECK(line.find("still alive") == std::string::npos);
    }
}

TEST_CASE("shutdown reports a script object that is still alive, and destroying it afterwards is harmless")
{
    TempDir dir;
    dir.write("leak.py", game_script("'layer-leak'"));
    ScopedScriptingShutdown shutdown_scope;
    CoreLogCapture log;

    UniquePtr<IGame> game;
    {
        LayerStack stack;
        push_python_layer(stack, dir);
        game = create_game("layer-leak");
        REQUIRE(game != nullptr);
    }
    shutdown();

    std::vector<std::string> lines = log.lines();
    CHECK(std::count_if(lines.begin(), lines.end(), [](const std::string& line) { return line.find("still alive at stop") != std::string::npos; }) == 1);

    CHECK_NOTHROW(game.reset());
    CHECK(python::live_script_objects() == 0);
}

TEST_CASE("a script game that outlives the interpreter throws a ScriptError when it is used, instead of crashing")
{
    TempDir dir;
    dir.write("after.py", game_script("'layer-after-stop'"));
    ScopedScriptingShutdown shutdown_scope;
    CoreLogCapture log;

    UniquePtr<IGame> game;
    {
        LayerStack stack;
        push_python_layer(stack, dir);
        game = create_game("layer-after-stop");
        REQUIRE(game != nullptr);
    }
    shutdown();

    CHECK_THROWS_WITH_AS(game->new_initial_state(), doctest::Contains("not running"), ScriptError);
    CHECK_NOTHROW(game.reset());
}

TEST_CASE("the embedded interpreter sees the venv's site-packages")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write("paths.py", marker_prelude(marker) +
        "import sys\nmark('|'.join(p for p in sys.path if p.endswith('site-packages')))\n");

    RunningPython python;
    python.load(script);

    CHECK(read_file(marker).find("site-packages") != std::string::npos);
}

#endif
