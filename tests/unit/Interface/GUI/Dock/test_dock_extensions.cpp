#include "unit/Interface/GUI/Dock/DockTestSupport.h"

#include "Oryx/Interface/GUI/GuiSettings.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

struct TempDir
{
    std::filesystem::path path = std::filesystem::temp_directory_path() / "oryx_dock_extensions_test";

    TempDir()
    {
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
    }

    ~TempDir() { std::filesystem::remove_all(path); }
};

void register_viewport()
{
    gui::PanelOptions options;
    options.kind = gui::PanelKind::Viewport;
    REQUIRE(gui::register_panel("vp", options));
}

}

TEST_CASE("dock yaml: extension sections round-trip and nothing else reads them")
{
    const gui::PanelTable panels = make_panels();
    const gui::DockLayout layout = make_sample(panels);
    gui::LayoutExtensions extensions;
    extensions["workbench.notes"] = "version: 1\nlevel: 3\n";
    extensions["other"] = "a: [1, 2]";

    std::string text;
    REQUIRE(gui::serialize_layout_to_string(layout, panels, text, &extensions));
    CHECK(text.find("extensions:") != std::string::npos);

    gui::DockLayout read;
    gui::LayoutExtensions back;
    REQUIRE(gui::deserialize_layout_from_string(read, panels, text, nullptr, &back));
    CHECK(gui::equal(read, layout));
    REQUIRE(back.size() == 2);
    CHECK(back["workbench.notes"].find("level: 3") != std::string::npos);

    gui::DockLayout plain;
    REQUIRE(gui::deserialize_layout_from_string(plain, panels, text));
    CHECK(gui::equal(plain, layout));

    std::string without;
    REQUIRE(gui::serialize_layout_to_string(layout, panels, without));
    CHECK(without.find("extensions") == std::string::npos);
}

TEST_CASE("dock yaml: an unparsable extension is skipped, the layout still saves")
{
    const gui::PanelTable panels = make_panels();
    const gui::DockLayout layout = make_sample(panels);
    gui::LayoutExtensions extensions;
    extensions["bad"] = "{{{";
    extensions["good"] = "x: 1";
    std::string text;
    REQUIRE(gui::serialize_layout_to_string(layout, panels, text, &extensions));
    gui::DockLayout read;
    gui::LayoutExtensions back;
    REQUIRE(gui::deserialize_layout_from_string(read, panels, text, nullptr, &back));
    CHECK(back.count("good") == 1);
    CHECK(back.count("bad") == 0);
}

TEST_CASE("the layout file holds the extensions: a set is saved, a later run reads it, a newer file is never rewritten")
{
    TempDir dir;
    const std::filesystem::path file = dir.path / "layout.yaml";
    update_settings<GuiSettings>([&file](GuiSettings& settings) { settings.layout_file = file; });
    {
        GuiContext context;
        ContextScope<GuiContext> scope(context);
        register_viewport();
        CHECK(gui::layout_extension("workbench.notes").empty());
        gui::set_layout_extension("workbench.notes", "version: 1\nlevel: 7\n");
        CHECK(gui::layout_extension("workbench.notes").find("level: 7") != std::string_view::npos);
    }
    REQUIRE(std::filesystem::exists(file));
    {
        GuiContext context;
        ContextScope<GuiContext> scope(context);
        register_viewport();
        CHECK(gui::layout_extension("workbench.notes").find("level: 7") != std::string_view::npos);
        gui::set_layout_extension("workbench.notes", "");
        CHECK(gui::layout_extension("workbench.notes").empty());
    }
    {
        GuiContext context;
        ContextScope<GuiContext> scope(context);
        register_viewport();
        CHECK(gui::layout_extension("workbench.notes").empty());
    }

    std::string newer;
    {
        std::ifstream in(file);
        newer.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    INFO(newer);
    const size_t at = newer.find("version: 1");
    REQUIRE(at != std::string::npos);
    newer.replace(at, 10, "version: 99");
    { std::ofstream(file) << newer; }
    {
        GuiContext context;
        ContextScope<GuiContext> scope(context);
        register_viewport();
        gui::set_layout_extension("workbench.notes", "version: 1\nlevel: 1\n");
    }
    std::ifstream in(file);
    const std::string after((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(after == newer);
    reset_settings();
}

TEST_CASE("layout extensions stay in memory without a layout file")
{
    update_settings<GuiSettings>([](GuiSettings& settings) { settings.layout_file.clear(); });
    GuiContext context;
    ContextScope<GuiContext> scope(context);
    register_viewport();
    gui::set_layout_extension("k", "v: 1");
    CHECK(gui::layout_extension("k") == "v: 1");
    gui::set_layout_extension("k", "");
    CHECK(gui::layout_extension("k").empty());
    reset_settings();
}
