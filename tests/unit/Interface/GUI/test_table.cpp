#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;
using test::GuiFixture;

namespace
{

constexpr float k_row_height = 20.0f;

struct TableScene
{
    GuiFixture f;
    uint32_t row_count = 1000;
    int32_t selected = -1;
    std::vector<uint32_t> submitted;
    gui::TableResult head;
    uint32_t clicked = gui::k_no_index;

    TableScene()
    {
        f.driver.input().surface_size = { 300.0f, 300.0f };
        submitted.reserve(64);
    }

    void body()
    {
        gui::TableOptions options;
        options.row_count = row_count;
        options.row_height = k_row_height;
        options.height = fixed(100.0f);
        gui::TableScope table("trace", options);
        table.column("Ply", fixed(50.0f), true);
        table.column("Move", grow(), true);
        head = table.headers();
        submitted.clear();
        for (uint32_t row = table.first_row(); row < table.last_row(); ++row)
        {
            submitted.push_back(row);
            const ItemState state = table.row(row, static_cast<int32_t>(row) == selected);
            if (state.clicked)
            {
                clicked = row;
                selected = static_cast<int32_t>(row);
            }
            table.cell(f.context.arena().format("%u", row));
            table.cell(f.context.arena().format("move %u", row));
        }
    }

    void settle() { f.driver.settle(f.column_of([this] { body(); })); }
    void frames(uint32_t count) { f.driver.run_frames(count, f.column_of([this] { body(); })); }
    void frame() { f.driver.frame(f.column_of([this] { body(); })); }

    void scroll_lines(float lines)
    {
        f.driver.move_to({ 100.0f, 60.0f });
        f.driver.wheel({ 0.0f, -lines });
        frames(3);
    }

    Vec2f content() const
    {
        Vec2f size;
        REQUIRE(f.context.layout().content_size_of(make_im_id("body", f.context.id("trace")), size));
        return size;
    }
};

} // namespace

TEST_CASE("GUI table: only the visible rows are submitted and the scroll range covers all of them")
{
    TableScene s;
    s.settle();
    REQUIRE_FALSE(s.submitted.empty());
    CHECK(s.submitted.front() == 0);
    CHECK(s.submitted.size() < 10);
    CHECK(s.content()[1] == doctest::Approx(1000.0f * k_row_height));
    CHECK(s.f.find_text("move 0") != nullptr);
    CHECK(s.f.find_text("move 500") == nullptr);
}

TEST_CASE("GUI table: rows follow the scroll offset to the middle and to the end")
{
    TableScene s;
    s.settle();
    s.scroll_lines(50.0f);
    REQUIRE_FALSE(s.submitted.empty());
    CHECK(s.submitted.front() >= 97);
    CHECK(s.submitted.front() <= 100);
    CHECK(s.submitted.size() < 10);
    CHECK(s.f.find_text("move 0") == nullptr);
    CHECK(s.f.find_text("move 100") != nullptr);
    CHECK(s.content()[1] == doctest::Approx(1000.0f * k_row_height));

    s.scroll_lines(100000.0f);
    CHECK(s.submitted.back() == 999);
    CHECK(s.f.find_text("move 999") != nullptr);
    CHECK(s.f.find_text("move 100") == nullptr);
}

TEST_CASE("GUI table: a sortable header toggles the direction and reports the column")
{
    TableScene s;
    s.settle();
    CHECK(s.head.sort_column == gui::k_no_index);
    s.f.driver.click(rect_centre(s.f.find_text("Move")->rect), [&] { s.body(); });
    CHECK(s.head.sort_column == 1);
    CHECK(s.head.sort_ascending);
    s.settle();
    CHECK(s.f.find_text("Move ^") != nullptr);
    s.f.driver.click(rect_centre(s.f.find_text("Move ^")->rect), s.f.column_of([&] { s.body(); }));
    s.settle();
    CHECK_FALSE(s.head.sort_ascending);
    CHECK(s.f.find_text("Move v") != nullptr);
    s.f.driver.click(rect_centre(s.f.find_text("Ply")->rect), s.f.column_of([&] { s.body(); }));
    s.settle();
    CHECK(s.head.sort_column == 0);
    CHECK(s.head.sort_ascending);
}

TEST_CASE("GUI table: a click on a row selects it")
{
    TableScene s;
    s.settle();
    s.f.driver.click(rect_centre(s.f.find_text("move 2")->rect), s.f.column_of([&] { s.body(); }));
    CHECK(s.clicked == 2);
    CHECK(s.selected == 2);
}

TEST_CASE("GUI table: shrinking the table re-clamps the scroll")
{
    TableScene s;
    s.settle();
    s.scroll_lines(50.0f);
    CHECK(s.submitted.front() > 50);
    s.row_count = 5;
    s.frames(4);
    REQUIRE_FALSE(s.submitted.empty());
    CHECK(s.submitted.front() == 0);
    CHECK(s.submitted.back() == 4);
}

TEST_CASE("GUI table: misuse throws")
{
    GuiFixture f;
    f.driver.frame(f.frame_of([] {}));
    const auto expect_throw = [&](auto&& body)
    {
        f.context.begin_frame(f.driver.input());
        {
            gui::BoxScope root("root", LayoutStyle{});
            CHECK_THROWS_AS(body(), Error);
        }
        f.context.abort_frame();
    };
    expect_throw([&] { gui::TableScope table("t", {}); table.row(0); });
    expect_throw([&] { gui::TableScope table("t", {}); table.headers(); table.headers(); });
    expect_throw([&]
    {
        gui::TableScope table("t", { .row_count = 1 });
        table.column("a");
        std::ignore = table.headers();
        std::ignore = table.row(0);
        table.cell("one");
        table.cell("two");
    });
    expect_throw([&]
    {
        gui::TableScope table("t", {});
        for (uint32_t index = 0; index <= gui::k_max_table_columns; ++index)
        {
            table.column("c");
        }
    });
}

TEST_CASE("GUI table: warm frames allocate nothing, scrolling included")
{
    TableScene s;
    s.settle();
    s.scroll_lines(5.0f);
    s.scroll_lines(5.0f);
    s.frames(4);
    const MemoryStats before = test::all_allocations();
    s.scroll_lines(5.0f);
    s.scroll_lines(5.0f);
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}
