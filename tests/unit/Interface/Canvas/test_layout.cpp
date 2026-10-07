#include "doctest.h"

#include "Oryx.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;

namespace
{

const Colour RED = { 1.0f, 0.0f, 0.0f, 1.0f };

float eight_per_character(void*, std::string_view text, float)
{
    return static_cast<float>(text.size()) * 8.0f;
}

const TextMeasure k_measure = { eight_per_character, nullptr };

LayoutStyle box_style(Sizing width, Sizing height, Direction direction = Direction::Row)
{
    LayoutStyle style;
    style.width = width;
    style.height = height;
    style.direction = direction;
    return style;
}

ImId named(const char* label)
{
    return make_im_id(label);
}

void solve(LayoutTree& tree, float width, float height, uint64_t frame = 1)
{
    tree.solve({ { 0.0f, 0.0f }, { width, height } }, k_measure, frame);
}

Rect rect_named(const LayoutTree& tree, const char* label)
{
    Rect rect;
    REQUIRE(tree.rect_of(named(label), rect));
    return rect;
}

void leaf_text(LayoutTree& tree, const char* label, std::string_view text, float height = 16.0f)
{
    const uint32_t index = tree.leaf(named(label), box_style(fit(), fit()), label);
    tree.node(index).paint.text = text;
    tree.node(index).paint.text_height = height;
}

} // namespace

TEST_CASE("Layout: grow splits the leftover by weight after fixed boxes")
{
    LayoutTree tree;
    tree.begin_box(named("root"), box_style(grow(), grow()));
    tree.leaf(named("fixed"), box_style(fixed(50.0f), fixed(10.0f)));
    tree.leaf(named("one"), box_style(grow(1.0f), fixed(10.0f)));
    tree.leaf(named("two"), box_style(grow(2.0f), fixed(10.0f)));
    tree.end_box();
    solve(tree, 300.0f, 100.0f);
    CHECK(rect_named(tree, "fixed") == Rect{ { 0.0f, 0.0f }, { 50.0f, 10.0f } });
    CHECK(rect_named(tree, "one").size[0] == doctest::Approx(250.0f / 3.0f));
    CHECK(rect_named(tree, "two").size[0] == doctest::Approx(500.0f / 3.0f));
    CHECK(rect_named(tree, "two").min[0] == doctest::Approx(50.0f + 250.0f / 3.0f));
}

TEST_CASE("Layout: grow stops at its max and the rest goes to the others")
{
    LayoutTree tree;
    tree.begin_box(named("root"), box_style(grow(), grow()));
    tree.leaf(named("capped"), box_style(grow(1.0f, 0.0f, 50.0f), fixed(10.0f)));
    tree.leaf(named("free"), box_style(grow(), fixed(10.0f)));
    tree.end_box();
    solve(tree, 200.0f, 100.0f);
    CHECK(rect_named(tree, "capped").size[0] == doctest::Approx(50.0f));
    CHECK(rect_named(tree, "free").size[0] == doctest::Approx(150.0f));
}

TEST_CASE("Layout: overflowing fit boxes compress the largest first and never below their min")
{
    LayoutTree tree;
    tree.begin_box(named("root"), box_style(fixed(100.0f), fixed(20.0f)));
    leaf_text(tree, "long", "AAAAAAAAAA");
    leaf_text(tree, "short", "AAAAAA");
    tree.end_box();
    solve(tree, 300.0f, 100.0f);
    CHECK(rect_named(tree, "long").size[0] == doctest::Approx(52.0f));
    CHECK(rect_named(tree, "short").size[0] == doctest::Approx(48.0f));

    LayoutTree floor;
    floor.begin_box(named("root"), box_style(fixed(100.0f), fixed(20.0f)));
    floor.leaf(named("a"), box_style(grow(1.0f, 80.0f), fixed(10.0f)));
    floor.leaf(named("b"), box_style(grow(1.0f, 80.0f), fixed(10.0f)));
    floor.end_box();
    solve(floor, 300.0f, 100.0f);
    CHECK(rect_named(floor, "a").size[0] == doctest::Approx(80.0f));
    CHECK(rect_named(floor, "b").min[0] == doctest::Approx(80.0f));
}

TEST_CASE("Layout: padding and gap place column children")
{
    LayoutTree tree;
    LayoutStyle root = box_style(fixed(100.0f), fixed(200.0f), Direction::Column);
    root.padding = uniform_insets(10.0f);
    root.gap = 5.0f;
    tree.begin_box(named("root"), root);
    tree.leaf(named("a"), box_style(fixed(20.0f), fixed(20.0f)));
    tree.leaf(named("b"), box_style(fixed(20.0f), fixed(20.0f)));
    tree.end_box();
    solve(tree, 500.0f, 500.0f);
    CHECK(rect_named(tree, "a") == Rect{ { 10.0f, 10.0f }, { 20.0f, 20.0f } });
    CHECK(rect_named(tree, "b") == Rect{ { 10.0f, 35.0f }, { 20.0f, 20.0f } });
}

TEST_CASE("Layout: a fit box wraps its text and padding")
{
    LayoutTree tree;
    LayoutStyle root = box_style(fit(), fit());
    root.padding = uniform_insets(4.0f);
    tree.begin_box(named("root"), root);
    leaf_text(tree, "text", "AB");
    tree.end_box();
    solve(tree, 500.0f, 500.0f);
    CHECK(rect_named(tree, "root").size == Vec2f(24.0f, 24.0f));
    CHECK(rect_named(tree, "text") == Rect{ { 4.0f, 4.0f }, { 16.0f, 16.0f } });
}

TEST_CASE("Layout: text measures as zero width without a measurer")
{
    LayoutTree tree;
    tree.begin_box(named("root"), box_style(fit(), fit()));
    leaf_text(tree, "text", "ABCD");
    tree.end_box();
    tree.solve({ { 0.0f, 0.0f }, { 100.0f, 100.0f } }, TextMeasure{}, 1);
    CHECK(rect_named(tree, "text").size[0] == 0.0f);
    CHECK(rect_named(tree, "text").size[1] == 16.0f);
}

TEST_CASE("Layout: alignment places children in the leftover on each axis")
{
    const Align aligns[] = { Align::Start, Align::Centre, Align::End };
    const float expected[] = { 0.0f, 40.0f, 80.0f };
    for (uint32_t x = 0; x < 3; ++x)
    {
        for (uint32_t y = 0; y < 3; ++y)
        {
            LayoutTree tree;
            LayoutStyle root = box_style(fixed(100.0f), fixed(100.0f));
            root.align_x = aligns[x];
            root.align_y = aligns[y];
            tree.begin_box(named("root"), root);
            tree.leaf(named("child"), box_style(fixed(20.0f), fixed(20.0f)));
            tree.end_box();
            solve(tree, 500.0f, 500.0f);
            CHECK(rect_named(tree, "child").min == Vec2f(expected[x], expected[y]));
        }
    }
}

TEST_CASE("Layout: percent resolves against the parent's content box and min/max clamp it")
{
    LayoutTree tree;
    LayoutStyle root = box_style(fixed(200.0f), fixed(100.0f));
    root.padding = uniform_insets(10.0f);
    tree.begin_box(named("root"), root);
    tree.leaf(named("half"), box_style(percent(0.5f), percent(1.0f)));
    Sizing clamped = percent(1.0f);
    clamped.max = 30.0f;
    tree.leaf(named("clamped"), box_style(clamped, fixed(5.0f)));
    tree.end_box();
    solve(tree, 500.0f, 500.0f);
    CHECK(rect_named(tree, "half") == Rect{ { 10.0f, 10.0f }, { 90.0f, 80.0f } });
    CHECK(rect_named(tree, "clamped").size[0] == 30.0f);
}

TEST_CASE("Layout: a percent box inside a fit parent adds nothing to its fit size")
{
    LayoutTree tree;
    tree.begin_box(named("root"), box_style(fit(), fit()));
    tree.leaf(named("half"), box_style(percent(0.5f), fixed(10.0f)));
    tree.end_box();
    solve(tree, 500.0f, 500.0f);
    CHECK(rect_named(tree, "root").size[0] == 0.0f);
}

TEST_CASE("Layout: aspect ratio derives the free axis or fits two flexible ones")
{
    LayoutStyle derived = box_style(fixed(200.0f), fit());
    derived.aspect_ratio = 2.0f;
    LayoutTree tree;
    tree.begin_box(named("derived"), derived);
    tree.end_box();
    LayoutStyle contained = box_style(grow(), grow());
    contained.aspect_ratio = 1.0f;
    tree.begin_box(named("outer"), box_style(fixed(300.0f), fixed(100.0f)));
    tree.leaf(named("square"), contained);
    tree.end_box();
    solve(tree, 500.0f, 500.0f);
    CHECK(rect_named(tree, "derived").size == Vec2f(200.0f, 100.0f));
    CHECK(rect_named(tree, "square").size == Vec2f(100.0f, 100.0f));
}

TEST_CASE("Layout: floating boxes attach by point pair to the root, the parent or an element")
{
    for (const Vec2f& viewport : { Vec2f(200.0f, 100.0f), Vec2f(400.0f, 300.0f) })
    {
        LayoutTree tree;
        tree.begin_box(named("root"), box_style(grow(), grow()));
        tree.leaf(named("anchor"), box_style(fixed(60.0f), fixed(20.0f)));
        LayoutStyle toast = box_style(fixed(20.0f), fixed(10.0f));
        toast.floating = { true, AttachPoint::BottomCentre, AttachPoint::BottomCentre, FloatTarget::Root };
        tree.leaf(named("toast"), toast);
        LayoutStyle tip = box_style(fixed(10.0f), fixed(10.0f));
        tip.floating = { true, AttachPoint::TopLeft, AttachPoint::BottomRight, FloatTarget::Element, named("anchor"), { 2.0f, 3.0f } };
        tree.leaf(named("tip"), tip);
        tree.end_box();
        solve(tree, viewport[0], viewport[1]);
        CHECK(rect_named(tree, "toast") == Rect{ { viewport[0] * 0.5f - 10.0f, viewport[1] - 10.0f }, { 20.0f, 10.0f } });
        CHECK(rect_named(tree, "tip").min == Vec2f(62.0f, 23.0f));
        CHECK(rect_named(tree, "anchor") == Rect{ { 0.0f, 0.0f }, { 60.0f, 20.0f } });
    }
}

TEST_CASE("Layout: a floating box takes no room in the flow and a missing target throws")
{
    LayoutTree tree;
    tree.begin_box(named("root"), box_style(grow(), fit()));
    LayoutStyle floating = box_style(fixed(500.0f), fixed(500.0f));
    floating.floating.enabled = true;
    tree.leaf(named("float"), floating);
    tree.leaf(named("flow"), box_style(fixed(10.0f), fixed(10.0f)));
    tree.end_box();
    solve(tree, 100.0f, 100.0f);
    CHECK(rect_named(tree, "root").size == Vec2f(100.0f, 10.0f));

    LayoutTree broken;
    LayoutStyle lost = box_style(fixed(1.0f), fixed(1.0f));
    lost.floating = { true, AttachPoint::TopLeft, AttachPoint::TopLeft, FloatTarget::Element, named("nothing") };
    broken.leaf(named("lost"), lost);
    CHECK_THROWS_AS(solve(broken, 10.0f, 10.0f), Error);
}

TEST_CASE("Layout: unbalanced boxes throw and clear recovers")
{
    LayoutTree tree;
    CHECK_THROWS_AS(tree.end_box(), Error);
    CHECK_THROWS_AS(tree.current(), Error);
    tree.begin_box(named("root"), box_style(grow(), grow()));
    CHECK_THROWS_AS(solve(tree, 10.0f, 10.0f), Error);
    tree.clear();
    CHECK_NOTHROW(solve(tree, 10.0f, 10.0f));
}

TEST_CASE("Layout: rects of boxes with an id are remembered and forgotten when they vanish")
{
    LayoutTree tree;
    tree.begin_box(named("root"), box_style(fixed(10.0f), fixed(10.0f)));
    tree.end_box();
    solve(tree, 100.0f, 100.0f, 1);
    Rect rect;
    CHECK(tree.rect_of(named("root"), rect));
    CHECK_FALSE(tree.rect_of(named("other"), rect));
    tree.clear();
    tree.begin_box(ImId{}, box_style(fixed(10.0f), fixed(10.0f)));
    tree.end_box();
    solve(tree, 100.0f, 100.0f, 2);
    CHECK_FALSE(tree.rect_of(named("root"), rect));
}

TEST_CASE("Layout: solving twice gives the same dump")
{
    LayoutTree tree;
    tree.begin_box(named("root"), box_style(grow(), grow(), Direction::Column));
    tree.leaf(named("a"), box_style(grow(), grow(2.0f)));
    tree.leaf(named("b"), box_style(grow(), grow()));
    tree.end_box();
    solve(tree, 300.0f, 100.0f);
    const std::string first = dump_layout(tree);
    solve(tree, 300.0f, 100.0f);
    CHECK(dump_layout(tree) == first);
}

TEST_CASE("Layout: paint records fills, clips children and orders channels")
{
    LayoutTree tree;
    LayoutStyle panel = box_style(fixed(50.0f), fixed(50.0f));
    panel.overflow = Overflow::Clip;
    LayoutStyle popup = box_style(fixed(20.0f), fixed(20.0f));
    popup.channel = 1;
    const uint32_t root = tree.begin_box(named("root"), box_style(grow(), grow()));
    tree.node(root).paint.has_fill = true;
    tree.node(root).paint.fill = RED;
    const uint32_t clipped = tree.begin_box(named("panel"), panel);
    tree.node(clipped).paint.has_fill = true;
    const uint32_t inside = tree.leaf(named("inside"), box_style(fixed(80.0f), fixed(80.0f)));
    tree.node(inside).paint.has_fill = true;
    tree.end_box();
    const uint32_t over = tree.leaf(named("popup"), popup);
    tree.node(over).paint.has_fill = true;
    tree.end_box();
    solve(tree, 200.0f, 100.0f);
    DrawList list;
    tree.paint(list, nullptr, 1.0f);
    REQUIRE(list.channel_count() == 1);
    const std::vector<RectCmd>& rects = list.channel(0).rects;
    REQUIRE(rects.size() == 4);
    CHECK(rects[0].clip == k_no_clip);
    CHECK(list.clip(rects[2].clip) == Rect{ { 0.0f, 0.0f }, { 50.0f, 50.0f } });
    CHECK(rects[3].rect == Rect{ { 50.0f, 0.0f }, { 20.0f, 20.0f } });
}

TEST_CASE("Layout: golden dump of a board overlay")
{
    LayoutTree tree;
    tree.begin_box(named("root"), box_style(grow(), grow(), Direction::Column), "root");
    tree.leaf(named("board"), box_style(grow(), grow()), "board");
    LayoutStyle menu = box_style(grow(), fit());
    menu.padding = uniform_insets(8.0f);
    menu.gap = 8.0f;
    menu.align_x = Align::Centre;
    tree.begin_box(named("menu"), menu, "menu");
    tree.leaf(named("b0"), box_style(fixed(100.0f), fixed(32.0f)), "b0");
    tree.leaf(named("b1"), box_style(fixed(100.0f), fixed(32.0f)), "b1");
    tree.leaf(named("b2"), box_style(fixed(100.0f), fixed(32.0f)), "b2");
    tree.end_box();
    LayoutStyle status = box_style(fit(), fit());
    status.floating = { true, AttachPoint::TopCentre, AttachPoint::TopCentre, FloatTarget::Parent, {}, { 0.0f, 8.0f } };
    const uint32_t line = tree.leaf(named("status"), status, "status");
    tree.node(line).paint.text = "Your turn";
    tree.end_box();
    solve(tree, 800.0f, 600.0f);
    CHECK(dump_layout(tree) ==
          "root [0.00 0.00 800.00 600.00] column w=grow(1.00) h=grow(1.00)\n"
          "  board [0.00 0.00 800.00 552.00] row w=grow(1.00) h=grow(1.00)\n"
          "  menu [0.00 552.00 800.00 48.00] row w=grow(1.00) h=fit\n"
          "    b0 [242.00 560.00 100.00 32.00] row w=fixed(100.00) h=fixed(32.00)\n"
          "    b1 [350.00 560.00 100.00 32.00] row w=fixed(100.00) h=fixed(32.00)\n"
          "    b2 [458.00 560.00 100.00 32.00] row w=fixed(100.00) h=fixed(32.00)\n"
          "  status [364.00 8.00 72.00 16.00] row w=fit h=fit floating text=\"Your turn\"\n");
}

TEST_CASE("Layout: a warm tree solves and paints without allocating")
{
    LayoutTree tree;
    DrawList list;
    auto frame = [&]()
    {
        tree.clear();
        tree.begin_box(named("root"), box_style(grow(), grow()));
        for (uint32_t i = 0; i < 20; ++i)
        {
            const uint32_t index = tree.leaf(make_im_index_id(i, named("root")), box_style(grow(), fixed(10.0f)));
            tree.node(index).paint.has_fill = true;
        }
        tree.end_box();
        solve(tree, 400.0f, 100.0f, 1);
        list.clear();
        tree.paint(list, nullptr, 1.0f);
    };
    frame();
    frame();
    MemoryStats before = test::all_allocations();
    frame();
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}

namespace
{

struct TestContext : ImContext
{
};

ImInput press_at(float x, float y, bool pressed)
{
    ImInput input;
    input.surface_size = { 200.0f, 100.0f };
    input.pointer.valid = true;
    input.pointer.position = { x, y };
    input.pointer.buttons[static_cast<uint32_t>(MouseCode::Left)] = { pressed, pressed, false };
    return input;
}

ItemState button_frame(TestContext& context, const ImInput& input, ImId& id)
{
    context.begin_frame(input);
    context.begin_box("root", box_style(grow(), grow()));
    id = context.id("button");
    context.begin_box(id, box_style(fixed(40.0f), fixed(20.0f)));
    const ItemState state = context.item(id);
    context.end_box();
    context.end_box();
    context.end_frame();
    return state;
}

} // namespace

TEST_CASE("ImContext layout: hit-testing uses last frame's solved rect, so the first frame has no hit")
{
    TestContext context;
    ImId id;
    CHECK_FALSE(button_frame(context, press_at(10.0f, 10.0f, true), id).hovered);
    ItemState second = button_frame(context, press_at(10.0f, 10.0f, true), id);
    CHECK(second.hovered);
    CHECK(second.pressed);
    CHECK_FALSE(button_frame(context, press_at(100.0f, 50.0f, false), id).hovered);
    context.begin_frame(press_at(0.0f, 0.0f, false));
    Rect previous;
    CHECK(context.previous_rect(id, previous));
    CHECK(previous == Rect{ { 0.0f, 0.0f }, { 40.0f, 20.0f } });
    context.end_frame();
}

TEST_CASE("ImContext layout: an open box fails end_frame and abort_frame recovers")
{
    TestContext context;
    context.begin_frame(press_at(0.0f, 0.0f, false));
    context.begin_box("root", box_style(grow(), grow()));
    CHECK_THROWS_AS(context.end_frame(), Error);
    context.abort_frame();
    context.begin_frame(press_at(0.0f, 0.0f, false));
    CHECK(context.layout().node_count() == 0);
    context.end_frame();
    CHECK_THROWS_AS(context.begin_box("late", box_style(grow(), grow())), Error);
}

TEST_CASE("ImContext layout: end_frame paints the boxes and dump_layout names them")
{
    TestContext context;
    context.begin_frame(press_at(0.0f, 0.0f, false));
    const uint32_t index = context.begin_box("panel", box_style(fixed(50.0f), fixed(20.0f)));
    context.layout().node(index).paint.has_fill = true;
    context.end_box();
    context.end_frame();
    CHECK(context.draw_list().channel(0).rects.size() == 1);
    CHECK(dump_layout(context).starts_with("panel [0.00 0.00 50.00 20.00]"));
    CHECK(context.stats().boxes == 1);
}
