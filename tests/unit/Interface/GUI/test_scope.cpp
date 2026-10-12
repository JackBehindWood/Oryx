#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/support/GuiFixture.h"

using namespace oryx;
using test::GuiFixture;

namespace
{

uint32_t g_ends = 0;

struct CountingEnd
{
    static void end() { ++g_ends; }
};

} // namespace

TEST_CASE("Scope: ends only when begin opened")
{
    g_ends = 0;
    {
        gui::Scope<CountingEnd> closed(false);
        CHECK_FALSE(closed.open());
        CHECK_FALSE(static_cast<bool>(closed));
    }
    CHECK(g_ends == 0);
    {
        gui::Scope<CountingEnd> open(true);
        CHECK(static_cast<bool>(open));
    }
    CHECK(g_ends == 1);
    {
        gui::AlwaysScope<CountingEnd> always;
    }
    CHECK(g_ends == 2);
}

TEST_CASE("Scope: a failing end during unwinding does not terminate")
{
    struct ThrowingEnd
    {
        static void end() { throw Error("end failed"); }
    };
    auto unwind = []
    {
        gui::Scope<ThrowingEnd> scope(true);
        throw Error("first");
    };
    CHECK_THROWS_WITH_AS(unwind(), "first", Error);
}

TEST_CASE("Scope: end without begin throws and names the call")
{
    GuiFixture f;
    f.driver.frame(f.column_of(
        [&]
        {
            CHECK_THROWS_AS(gui::end_table(), Error);
            CHECK_THROWS_AS(gui::end_plot(), Error);
            CHECK_THROWS_AS(gui::end_tab_bar(), Error);
            CHECK_THROWS_AS(gui::end_list_box(), Error);
            CHECK_THROWS_AS(gui::end_combo(), Error);
            CHECK_THROWS_AS(gui::end_field(), Error);
            CHECK_THROWS_AS(gui::end_menu(), Error);
            CHECK_THROWS_AS(gui::end_menu_bar(), Error);
            CHECK_THROWS_AS(gui::end_popup(), Error);
            CHECK_THROWS_AS(gui::end_modal(), Error);
            CHECK_THROWS_AS(gui::end_tooltip(), Error);
            CHECK_THROWS_AS(gui::end_tree_node(), Error);
        }));
}

TEST_CASE("Scope: a widget call outside its scope throws")
{
    GuiFixture f;
    f.driver.frame(f.column_of(
        [&]
        {
            CHECK_THROWS_AS(gui::table_headers(), Error);
            CHECK_THROWS_AS(gui::plot_hline(1.0f, {}), Error);
            CHECK_THROWS_AS(gui::tab_item("a"), Error);
            CHECK_THROWS_AS(gui::combo_item("a", false), Error);
        }));
}

TEST_CASE("Scope: ending the wrong kind throws and leaves the scope open")
{
    GuiFixture f;
    f.driver.frame(f.column_of(
        [&]
        {
            gui::TableOptions options;
            options.row_count = 1;
            gui::TableScope table("t", options);
            table.column("a");
            std::ignore = table.headers();
            CHECK_THROWS_WITH_AS(gui::end_plot(), doctest::Contains("table"), Error);
            CHECK_THROWS_AS(gui::end_tab_bar(), Error);
        }));
}

TEST_CASE("Scope: a scope left open fails end_frame and abort_frame recovers")
{
    GuiFixture f;
    f.context.begin_frame(f.driver.input());
    std::ignore = gui::begin_plot("p");
    CHECK_THROWS_WITH_AS(f.context.end_frame(), doctest::Contains("plot"), Error);
    f.context.abort_frame();
    CHECK(f.context.scope_depth() == 0);
    f.driver.frame(f.column_of([&] { gui::PlotScope plot("p"); }));
}

TEST_CASE("Scope: nesting past the stack limit throws")
{
    GuiFixture f;
    f.driver.frame(f.column_of(
        [&]
        {
            uint32_t opened = 0;
            CHECK_THROWS_AS(
                [&]
                {
                    for (uint32_t index = 0; index <= k_max_scopes; ++index)
                    {
                        gui::begin_field("f", {});
                        ++opened;
                    }
                }(),
                Error);
            for (uint32_t index = 0; index < opened; ++index)
            {
                gui::end_field();
            }
        }));
}

TEST_CASE("Scope: a closed combo and an open one both end cleanly")
{
    GuiFixture f;
    for (uint32_t frame = 0; frame < 3; ++frame)
    {
        f.driver.frame(f.column_of(
            [&]
            {
                gui::ComboScope combo("mode", "heat");
                if (combo)
                {
                    std::ignore = combo.item("heat", true);
                }
            }));
    }
    CHECK(f.context.scope_depth() == 0);
}

TEST_CASE("Scope: nested widgets keep their own state")
{
    GuiFixture f;
    f.driver.frame(f.column_of(
        [&]
        {
            gui::TableOptions options;
            options.row_count = 2;
            options.height = fixed(80.0f);
            gui::TableScope table("outer", options);
            table.column("a");
            std::ignore = table.headers();
            for (uint32_t row = table.first_row(); row < table.last_row(); ++row)
            {
                std::ignore = table.row(row);
                table.begin_cell();
                gui::PlotScope plot("inner");
                plot.hline(0.5f, { 1.0f, 0.0f, 0.0f, 1.0f });
                table.end_cell();
            }
        }));
    CHECK(f.context.scope_depth() == 0);
}
