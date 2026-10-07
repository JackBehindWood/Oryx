#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;
using test::GuiFixture;

namespace
{

float scroll_offset_at(const GuiFixture& f, uint32_t nth = 0)
{
    uint32_t seen = 0;
    for (uint32_t index = 0; index < f.context.layout().node_count(); ++index)
    {
        const LayoutNode& node = f.context.layout().node(index);
        if (node.style.overflow == Overflow::Scroll && seen++ == nth)
        {
            return node.style.scroll_offset[1];
        }
    }
    return -1.0f;
}

void rows(uint32_t count, const char* const* names)
{
    for (uint32_t index = 0; index < count; ++index)
    {
        gui::label(names[index]);
    }
}

constexpr const char* k_names[] = { "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9" };

} // namespace

TEST_CASE("GUI structure: a collapsing header toggles, keeps its state and forgets it after a skipped frame")
{
    GuiFixture f;
    bool open = false;
    const auto body = [&] { open = gui::collapsing_header("Section"); };
    f.driver.settle(f.column_of(body));
    CHECK_FALSE(open);
    f.driver.click(rect_centre(f.find_text("> Section")->rect), f.column_of(body));
    CHECK(open);
    f.driver.run_frames(3, f.column_of(body));
    CHECK(open);
    f.driver.frame([] {});
    f.driver.frame(f.column_of(body));
    CHECK_FALSE(open);
}

TEST_CASE("GUI structure: default_open starts a header open")
{
    GuiFixture f;
    bool open = false;
    f.driver.settle(f.column_of([&] { open = gui::collapsing_header("Section", true); }));
    CHECK(open);
}

TEST_CASE("GUI structure: a tree node opens its children, a leaf never does, and selection paints")
{
    GuiFixture f;
    bool root_open = false;
    bool leaf_open = true;
    ItemState leaf_item;
    const auto body = [&]
    {
        const gui::TreeNodeResult root = gui::begin_tree_node("root");
        root_open = root.open;
        if (root.open)
        {
            const gui::TreeNodeResult leaf = gui::begin_tree_node("leaf", { .leaf = true, .selected = true });
            leaf_open = leaf.open;
            leaf_item = leaf.item;
            gui::end_tree_node();
        }
    };
    f.driver.settle(f.column_of(body));
    CHECK_FALSE(root_open);
    f.driver.click(rect_centre(f.find_text("> root")->rect), f.column_of(body));
    CHECK(root_open);
    f.driver.settle(f.column_of(body));
    const LayoutNode* leaf = f.find_text("  leaf");
    REQUIRE(leaf != nullptr);
    CHECK(leaf->paint.has_fill);
    CHECK(approx_equal(leaf->paint.fill, f.theme.base.accent));
    const LayoutNode* root = f.find_text("v root");
    REQUIRE(root != nullptr);
    CHECK(leaf->rect.min[0] > root->rect.min[0]);
    f.driver.click(rect_centre(leaf->rect), f.column_of(body));
    CHECK_FALSE(leaf_open);
    CHECK(root_open);
}

TEST_CASE("GUI structure: the wheel scrolls a region, clamps, and re-clamps when the content shrinks")
{
    GuiFixture f;
    uint32_t count = 10;
    const auto body = [&]
    {
        gui::ScrollScope scope("list", { .height = fixed(50.0f) });
        rows(count, k_names);
    };
    f.driver.move_to({ 20.0f, 20.0f });
    f.driver.settle(f.column_of(body));
    CHECK(scroll_offset_at(f) == doctest::Approx(0.0f));
    f.driver.wheel({ 0.0f, -1.0f });
    f.driver.frame(f.column_of(body));
    f.driver.frame(f.column_of(body));
    CHECK(scroll_offset_at(f) == doctest::Approx(40.0f));
    for (uint32_t step = 0; step < 20; ++step)
    {
        f.driver.wheel({ 0.0f, -1.0f });
        f.driver.frame(f.column_of(body));
    }
    f.driver.frame(f.column_of(body));
    const float max_offset = scroll_offset_at(f);
    CHECK(max_offset > 100.0f);
    f.driver.wheel({ 0.0f, 1.0f });
    f.driver.run_frames(2, f.column_of(body));
    CHECK(scroll_offset_at(f) == doctest::Approx(max_offset - 40.0f));
    count = 3;
    f.driver.run_frames(3, f.column_of(body));
    CHECK(scroll_offset_at(f) < max_offset - 40.0f);
    CHECK(scroll_offset_at(f) >= 0.0f);
}

TEST_CASE("GUI structure: nested scroll regions give the wheel to the innermost that can move, then chain outward")
{
    GuiFixture f;
    f.driver.input().surface_size = { 200.0f, 200.0f };
    const auto body = [&]
    {
        gui::ScrollScope outer("outer", { .height = fixed(120.0f) });
        {
            gui::ScrollScope inner("inner", { .height = fixed(60.0f) });
            rows(5, k_names);
        }
        rows(5, k_names + 5);
    };
    f.driver.move_to({ 20.0f, 20.0f });
    f.driver.settle(f.column_of(body));
    f.driver.wheel({ 0.0f, -1.0f });
    f.driver.run_frames(2, f.column_of(body));
    CHECK(scroll_offset_at(f, 1) == doctest::Approx(40.0f));
    CHECK(scroll_offset_at(f, 0) == doctest::Approx(0.0f));
    for (uint32_t step = 0; step < 10; ++step)
    {
        f.driver.wheel({ 0.0f, -1.0f });
        f.driver.frame(f.column_of(body));
    }
    f.driver.run_frames(2, f.column_of(body));
    CHECK(scroll_offset_at(f, 0) > 0.0f);
}

TEST_CASE("GUI structure: dragging the scroll bar thumb moves the offset")
{
    GuiFixture f;
    const auto body = [&]
    {
        gui::ScrollScope scope("list", { .height = fixed(50.0f) });
        rows(10, k_names);
    };
    f.driver.settle(f.column_of(body));
    const LayoutNode* thumb = nullptr;
    for (uint32_t index = 0; index < f.context.layout().node_count(); ++index)
    {
        const LayoutNode& node = f.context.layout().node(index);
        if (node.style.floating.enabled && node.style.width.kind == SizingKind::Fixed && node.style.width.value == f.theme.scrollbar_width)
        {
            thumb = &node;
        }
    }
    REQUIRE(thumb != nullptr);
    const Vec2f start = rect_centre(thumb->rect);
    f.driver.drag(start, start + Vec2f(0.0f, 10.0f), 5, f.column_of(body));
    f.driver.frame(f.column_of(body));
    CHECK(scroll_offset_at(f) > 10.0f);
}

TEST_CASE("GUI structure: tab bar selects on a press and reports drag and close")
{
    GuiFixture f;
    uint32_t selected = 0;
    gui::TabBarResult last;
    const std::string_view labels[] = { "A", "B", "C" };
    bool closable = false;
    const auto body = [&] { last = gui::tab_bar("tabs", labels, selected, { .closable = closable }); };
    f.driver.settle(f.column_of(body));
    f.driver.move_to(rect_centre(f.find_text("B")->rect));
    f.driver.press();
    f.driver.frame(f.column_of(body));
    CHECK(last.changed);
    CHECK(last.pressed_index == 1);
    CHECK(selected == 1);
    f.driver.release();
    f.driver.frame(f.column_of(body));
    CHECK_FALSE(last.changed);
    f.driver.drag(rect_centre(f.find_text("C")->rect), rect_centre(f.find_text("C")->rect) + Vec2f(30.0f, 0.0f), 4, f.column_of(body));
    CHECK(selected == 2);

    closable = true;
    f.driver.settle(f.column_of(body));
    f.driver.move_to(rect_centre(f.find_text("x")->rect));
    f.driver.frame(f.column_of(body));
    f.driver.press();
    f.driver.frame(f.column_of(body));
    f.driver.release();
    f.driver.frame(f.column_of(body));
    CHECK(last.closed_index == 0);
    CHECK(selected == 2);
}

TEST_CASE("GUI structure: a splitter drags its first pane and clamps at both minimums")
{
    GuiFixture f;
    float first = 80.0f;
    const auto body = [&]
    {
        LayoutStyle pane;
        pane.width = fixed(first);
        pane.height = grow();
        gui::begin_box("left", pane);
        gui::end_box();
        std::ignore = gui::splitter("split", first);
    };
    const auto frame = f.frame_of(body);
    f.driver.settle(frame);
    const Vec2f grab(first + 2.0f, 50.0f);
    f.driver.drag(grab, grab + Vec2f(20.0f, 0.0f), 4, frame);
    CHECK(first == doctest::Approx(100.0f));
    f.driver.settle(frame);
    f.driver.drag({ first + 2.0f, 50.0f }, { first + 502.0f, 50.0f }, 4, frame);
    CHECK(first == doctest::Approx(156.0f));
    f.driver.settle(frame);
    f.driver.drag({ first + 2.0f, 50.0f }, { -400.0f, 50.0f }, 4, frame);
    CHECK(first == doctest::Approx(40.0f));
    f.driver.settle(frame);
    f.driver.move_to({ first + 2.0f, 50.0f });
    f.driver.frame(frame);
    CHECK(f.context.output().cursor == CursorShape::ResizeHorizontal);
}

TEST_CASE("GUI structure: a list box selects the clicked row")
{
    GuiFixture f;
    const std::string_view items[] = { "one", "two", "three" };
    int32_t selected = 0;
    bool changed = false;
    const auto body = [&] { changed = gui::list_box("items", items, selected) || changed; };
    f.driver.settle(f.column_of(body));
    f.driver.click(rect_centre(f.find_text("three")->rect), f.column_of(body));
    CHECK(selected == 2);
    CHECK(changed);
}

TEST_CASE("GUI structure: filter_matches is a case-insensitive substring test")
{
    CHECK(gui::filter_matches("", "anything"));
    CHECK(gui::filter_matches("MIN", "minimax/nodes"));
    CHECK(gui::filter_matches("nodes", "Minimax/Nodes"));
    CHECK_FALSE(gui::filter_matches("mcts", "minimax"));
    CHECK_FALSE(gui::filter_matches("longer than text", "text"));
}

TEST_CASE("GUI structure: warm frames allocate nothing")
{
    GuiFixture f;
    uint32_t selected = 0;
    int32_t item = 0;
    float first = 60.0f;
    const std::string_view labels[] = { "A", "B" };
    const std::string_view items[] = { "one", "two" };
    const auto body = [&]
    {
        std::ignore = gui::collapsing_header("Section", true);
        {
            gui::TreeScope tree("tree", { .default_open = true });
            gui::label("child");
        }
        {
            gui::ScrollScope scope("scroll", { .height = fixed(40.0f) });
            rows(8, k_names);
        }
        std::ignore = gui::tab_bar("tabs", labels, selected, { .closable = true });
        std::ignore = gui::list_box("items", items, item);
        gui::BoxScope row("row", LayoutStyle{});
        std::ignore = gui::splitter("split", first);
    };
    f.driver.move_to({ 20.0f, 20.0f });
    f.driver.run_frames(5, f.column_of(body));
    const MemoryStats before = test::all_allocations();
    f.driver.run_frames(3, f.column_of(body));
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}

TEST_CASE("GUI structure: golden layout of a menu bar, header and scroll panel")
{
    GuiFixture f;
    f.driver.input().surface_size = { 160.0f, 120.0f };
    const auto body = [&]
    {
        {
            gui::MenuBarScope bar("bar");
            gui::MenuScope file("File");
        }
        if (gui::collapsing_header("Stats", true))
        {
            gui::ScrollScope scope("list", { .height = fixed(40.0f) });
            rows(4, k_names);
        }
    };
    f.driver.settle(f.column_of(body));
    const std::string expected =
        "root [0.00 0.00 160.00 120.00] column w=grow(1.00) h=grow(1.00)\n"
        "  bar [0.00 0.00 160.00 28.00] row w=grow(1.00) h=fit\n"
        "    #03e4608196e89d51 [2.00 2.00 56.00 24.00] row w=fit h=fit text=\"File\"\n"
        "  Stats [0.00 28.00 160.00 24.00] row w=grow(1.00) h=fit text=\"v Stats\"\n"
        "  list [0.00 52.00 160.00 40.00] column w=grow(1.00) h=fixed(40.00) scroll(0.00 0.00 of 36.00 96.00)\n"
        "    r0 [0.00 52.00 36.00 24.00] row w=fit h=fit text=\"r0\"\n"
        "    r1 [0.00 76.00 36.00 24.00] row w=fit h=fit text=\"r1\"\n"
        "    r2 [0.00 100.00 36.00 24.00] row w=fit h=fit text=\"r2\"\n"
        "    r3 [0.00 124.00 36.00 24.00] row w=fit h=fit text=\"r3\"\n"
        "    #8b76bb748418875a [152.00 52.00 8.00 16.67] row w=fixed(8.00) h=fixed(16.67) floating\n";
    CHECK(dump_layout(f.context) == expected);
}
