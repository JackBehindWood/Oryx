#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;

namespace
{

struct ShowcaseFixture : test::GuiFixture
{
    ShowcaseFixture() { driver.input().surface_size = { 400.0f, 900.0f }; }

    void frames(uint32_t count = 3)
    {
        driver.run_frames(count, column_of([] { gui::show_showcase(); }));
    }

    void click(std::string_view text)
    {
        const LayoutNode* node = find_text(text);
        REQUIRE(node != nullptr);
        driver.click(rect_centre(node->rect), column_of([] { gui::show_showcase(); }));
        frames(2);
    }
};

} // namespace

TEST_CASE("showcase: the tabs draw and each opens its own sections")
{
    ShowcaseFixture f;
    f.frames();
    for (const char* tab : { "Widgets", "Inputs", "Data", "Style" })
    {
        CHECK_MESSAGE(f.find_text(tab) != nullptr, tab);
    }
    CHECK(f.find_text("basics") != nullptr);

    f.click("Inputs");
    CHECK(f.find_text("text field") != nullptr);
    f.click("Data");
    CHECK(f.find_text("policy heat grid") != nullptr);
    f.click("Style");
    CHECK(f.find_text("roles") != nullptr);
    f.click("Widgets");
    CHECK(f.find_text("basics") != nullptr);
}

TEST_CASE("showcase: a choice survives the frames and leaves the theme alone")
{
    ShowcaseFixture f;
    const GuiTheme before = gui::theme();
    f.frames();
    f.click("Inputs");
    f.frames(5);
    CHECK(f.find_text("text field") != nullptr);
    CHECK(gui::theme().panel.background == before.panel.background);
}

TEST_CASE("showcase: a warm frame allocates nothing")
{
    ShowcaseFixture f;
    f.frames(6);
    f.click("Data");
    f.frames(6);
    const MemoryStats before = test::all_allocations();
    f.frames(3);
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}
