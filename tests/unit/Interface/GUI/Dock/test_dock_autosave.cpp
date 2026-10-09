#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/GUI/Dock/DockTestSupport.h"
#include "unit/Interface/support/GuiFixture.h"

using namespace oryx;
using namespace oryx::gui;
using namespace oryx::test;

namespace
{

struct ScratchFile
{
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "oryx_dock_autosave_test";

    ScratchFile()
    {
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
    }

    ~ScratchFile() { std::filesystem::remove_all(dir); }

    [[nodiscard]] std::filesystem::path file() const { return dir / "layout.yaml"; }
};

} // namespace

TEST_CASE("dock autosave: nothing is written until the layout changes, then at once")
{
    ScratchFile scratch;
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    DockAutosave autosave(layout, true);

    CHECK_FALSE(autosave.update(layout, false, scratch.file(), panels));
    CHECK_FALSE(std::filesystem::exists(scratch.file()));

    REQUIRE(close_panel(layout, panels, make_panel_id("a")).applied);
    CHECK(autosave.update(layout, false, scratch.file(), panels));
    CHECK(std::filesystem::exists(scratch.file()));
    CHECK_FALSE(autosave.pending());
    CHECK_FALSE(autosave.update(layout, false, scratch.file(), panels));
}

TEST_CASE("dock autosave: a held drag waits for the release")
{
    ScratchFile scratch;
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    DockAutosave autosave(layout, true);

    REQUIRE(close_panel(layout, panels, make_panel_id("a")).applied);
    CHECK_FALSE(autosave.update(layout, true, scratch.file(), panels));
    CHECK_FALSE(std::filesystem::exists(scratch.file()));
    CHECK(autosave.pending());
    CHECK(autosave.update(layout, false, scratch.file(), panels));
    CHECK(std::filesystem::exists(scratch.file()));
}

TEST_CASE("dock autosave: a rebased layout is not a change and a newer-version file is never written")
{
    ScratchFile scratch;
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    DockAutosave autosave(layout, true);
    REQUIRE(close_panel(layout, panels, make_panel_id("a")).applied);
    autosave.rebase(layout);
    CHECK_FALSE(autosave.update(layout, false, scratch.file(), panels));
    CHECK_FALSE(std::filesystem::exists(scratch.file()));

    DockAutosave locked(layout, false);
    REQUIRE(close_panel(layout, panels, make_panel_id("b")).applied);
    CHECK_FALSE(locked.update(layout, false, scratch.file(), panels));
    CHECK_FALSE(locked.flush(layout, scratch.file(), panels));
    CHECK_FALSE(std::filesystem::exists(scratch.file()));
}

TEST_CASE("dock autosave: a failed write is retried on the next change and by flush")
{
    ScratchFile scratch;
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    DockAutosave autosave(layout, true);
    { std::ofstream(scratch.dir / "blocker") << "x"; }
    const std::filesystem::path blocked = scratch.dir / "blocker" / "layout.yaml";

    REQUIRE(close_panel(layout, panels, make_panel_id("a")).applied);
    CHECK_FALSE(autosave.update(layout, false, blocked, panels));
    CHECK(autosave.pending());
    CHECK(autosave.flush(layout, scratch.file(), panels));
    CHECK_FALSE(autosave.pending());
}

TEST_CASE("panel host: panel_rect reports any visible panel and panel_occluded sees floats above it")
{
    GuiFixture f;
    f.driver.input().surface_size = { 800.0f, 600.0f };
    f.context.dock_model().panels = make_panels();
    DockLayout& layout = f.context.dock_model().layout;
    layout = make_sample(f.context.dock_model().panels);
    settle_dock(f.context.dock_model());
    PanelOptions foreign;
    foreign.foreign_body = true;
    const auto draw = [&]
    {
        PanelHostScope host;
        for (const char* name : { "a", "b", "vp" })
        {
            PanelScope panel(name);
            if (panel.visible())
                gui::label("body");
        }
    };
    f.driver.run_frames(4, f.frame_of(draw));
    CHECK_FALSE(is_empty(panel_rect("b")));
    CHECK(is_empty(panel_rect("a")));
    CHECK(is_empty(panel_rect("nope")));
    const Rect vp = panel_rect("vp");
    CHECK_FALSE(panel_occluded("vp", rect_centre(vp)));
}
