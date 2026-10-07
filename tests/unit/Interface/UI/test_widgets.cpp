#include "doctest.h"

#include "Oryx.h"
#include "unit/MemoryTestSupport.h"
#include "unit/Renderer/FakeFontSource.h"

using namespace oryx;

namespace
{

constexpr uint32_t k_left = static_cast<uint32_t>(MouseCode::Left);

ImInput pointer(float x, float y, bool down = false, bool pressed = false, bool released = false)
{
    ImInput input;
    input.surface_size = { 200.0f, 100.0f };
    input.pointer.valid = true;
    input.pointer.position = { x, y };
    input.pointer.buttons[k_left] = { down, pressed, released };
    return input;
}

struct UiFixture
{
    test::FakeFontSource* source = nullptr;
    Font font = test::make_fake_font(source);
    UiContext context;
    UiTheme theme;
    ContextScope<UiContext> scope{ context };

    UiFixture()
    {
        theme.font = &font;
        context.set_theme(theme);
    }

    template<typename Body>
    void frame(const ImInput& input, Body&& body)
    {
        context.begin_frame(input);
        LayoutStyle root;
        root.width = grow();
        root.height = grow();
        ui::begin_box("root", root);
        body();
        ui::end_box();
        context.end_frame();
    }

    ItemState button_frame(const ImInput& input, const ui::WidgetOptions& options = {})
    {
        ItemState state;
        frame(input, [&] { state = ui::button("AB", options); });
        return state;
    }

    [[nodiscard]] const DrawChannel& channel() const { return context.draw_list().channel(0); }
};

} // namespace

TEST_CASE("UI: a button shows idle, hover and pressed colours and reacts one frame late")
{
    UiFixture f;
    CHECK_FALSE(f.button_frame(pointer(10.0f, 10.0f)).hovered);
    REQUIRE(f.channel().rounded_rects.size() == 1);
    CHECK(f.channel().rounded_rects[0].colour == f.theme.base.background);

    CHECK(f.button_frame(pointer(10.0f, 10.0f)).hovered);
    CHECK(f.channel().rounded_rects[0].colour == f.theme.base.hover);
    CHECK(f.channel().rounded_rects[0].rect == Rect{ { 0.0f, 0.0f }, { 36.0f, 24.0f } });

    const ItemState pressed = f.button_frame(pointer(10.0f, 10.0f, true, true));
    CHECK(pressed.pressed);
    CHECK(pressed.held);
    CHECK(f.channel().rounded_rects[0].colour == f.theme.base.pressed);
    REQUIRE(f.channel().borders.size() == 1);
    CHECK(f.channel().borders[0].colour == f.theme.base.border);
    REQUIRE(f.channel().texts.size() == 1);
    CHECK(f.context.draw_list().text(f.channel().texts[0]) == "AB");
}

TEST_CASE("UI: a click is reported once, on release over the button")
{
    UiFixture f;
    f.button_frame(pointer(10.0f, 10.0f));
    uint32_t clicks = 0;
    const ImInput frames[] = { pointer(10.0f, 10.0f, true, true), pointer(10.0f, 10.0f, true), pointer(10.0f, 10.0f, false, false, true), pointer(10.0f, 10.0f) };
    for (const ImInput& input : frames)
    {
        clicks += f.button_frame(input).clicked ? 1 : 0;
    }
    CHECK(clicks == 1);

    f.button_frame(pointer(10.0f, 10.0f, true, true));
    CHECK_FALSE(f.button_frame(pointer(150.0f, 80.0f, false, false, true)).clicked);
}

TEST_CASE("UI: a toggle flips on each click and fills with the accent while on")
{
    UiFixture f;
    bool value = false;
    const auto click = [&]
    {
        f.frame(pointer(10.0f, 10.0f, true, true), [&] { ui::toggle("AB", value); });
        f.frame(pointer(10.0f, 10.0f, false, false, true), [&] { ui::toggle("AB", value); });
        f.frame(pointer(150.0f, 80.0f), [&] { ui::toggle("AB", value); });
    };
    f.frame(pointer(10.0f, 10.0f), [&] { ui::toggle("AB", value); });
    click();
    CHECK(value);
    CHECK(f.channel().rounded_rects[0].colour == f.theme.base.accent);
    click();
    CHECK_FALSE(value);
    CHECK(f.channel().rounded_rects[0].colour == f.theme.base.background);
}

TEST_CASE("UI: a panel clips its children to its rect")
{
    UiFixture f;
    LayoutStyle size;
    size.width = fixed(50.0f);
    size.height = fixed(50.0f);
    f.frame(pointer(150.0f, 80.0f), [&]
    {
        ui::PanelScope panel("panel", { .layout = &size });
        ui::label("ABCDEABCDEABCDE");
    });
    REQUIRE(f.channel().texts.size() == 1);
    const Rect clip = f.context.draw_list().clip(f.channel().texts[0].clip);
    CHECK(clip.min[0] >= 0.0f);
    CHECK(rect_max(clip)[0] <= 50.0f);
    CHECK(rect_max(clip)[1] <= 50.0f);
    CHECK(f.context.draw_list().clip_depth() == 0);
}

TEST_CASE("UI: text in a narrow button is cut with an ellipsis")
{
    UiFixture f;
    LayoutStyle narrow;
    narrow.width = fixed(60.0f);
    narrow.height = fixed(24.0f);
    narrow.padding = f.theme.base.padding;
    f.frame(pointer(150.0f, 80.0f), [&] { std::ignore = ui::button("ABCDE", { .layout = &narrow }); });
    REQUIRE(f.channel().texts.size() == 1);
    const std::string_view shown = f.context.draw_list().text(f.channel().texts[0]);
    CHECK(shown.substr(shown.size() - 3) == "...");
    CHECK(shown.size() < 8);
}

TEST_CASE("UI: min_hit_size grows the hit area of a small button")
{
    UiFixture f;
    f.button_frame(pointer(37.0f, 12.0f));
    CHECK_FALSE(f.button_frame(pointer(37.0f, 12.0f)).hovered);

    UiFixture roomy;
    roomy.theme.min_hit_size = 40.0f;
    roomy.context.set_theme(roomy.theme);
    roomy.button_frame(pointer(37.0f, 12.0f));
    CHECK(roomy.button_frame(pointer(37.0f, 12.0f)).hovered);
}

TEST_CASE("UI: a style variant or a per-widget style changes the output")
{
    UiFixture f;
    ImStyle danger = f.theme.base;
    danger.background = { 0.8f, 0.1f, 0.1f, 1.0f };
    add_style_variant(f.theme, "danger", danger);
    f.context.set_theme(f.theme);
    f.button_frame(pointer(150.0f, 80.0f), { .variant = "danger" });
    CHECK(f.channel().rounded_rects[0].colour == danger.background);

    ImStyle custom = f.theme.base;
    custom.background = { 0.1f, 0.8f, 0.1f, 1.0f };
    custom.radius = 0.0f;
    f.button_frame(pointer(150.0f, 80.0f), { .variant = "danger", .style = &custom });
    REQUIRE(f.channel().rects.size() == 1);
    CHECK(f.channel().rects[0].colour == custom.background);
}

TEST_CASE("UI: the status line floats at the bottom centre with the status style")
{
    UiFixture f;
    f.frame(pointer(150.0f, 80.0f), [&] { ui::status_line("AB"); });
    Rect rect;
    REQUIRE(f.context.layout_rect(f.context.id("AB"), rect));
    CHECK(rect == Rect{ { 82.0f, 68.0f }, { 36.0f, 24.0f } });
    CHECK(f.channel().rounded_rects.empty());
    REQUIRE(f.channel().texts.size() == 1);

    f.frame(pointer(150.0f, 80.0f), [&] { ui::status_line("AB", { .at = AttachPoint::TopCentre }); });
    REQUIRE(f.context.layout_rect(f.context.id("AB"), rect));
    CHECK(rect.min == Vec2f(82.0f, 8.0f));
}

TEST_CASE("UI: an explicit status margin replaces the one derived from the padding")
{
    UiFixture f;
    f.frame(pointer(150.0f, 80.0f), [&] { ui::status_line("AB", { .at = AttachPoint::TopCentre, .margin = 20.0f }); });
    Rect rect;
    REQUIRE(f.context.layout_rect(f.context.id("AB"), rect));
    CHECK(rect.min == Vec2f(82.0f, 20.0f));

    f.frame(pointer(150.0f, 80.0f), [&] { ui::status_line("AB", { .at = AttachPoint::BottomCentre, .margin = 0.0f }); });
    REQUIRE(f.context.layout_rect(f.context.id("AB"), rect));
    CHECK(rect_max(rect)[1] == doctest::Approx(100.0f));
}

TEST_CASE("UI: a BoxScope closes its box and the frame can end")
{
    UiFixture f;
    LayoutStyle style;
    style.width = fixed(40.0f);
    style.height = fixed(20.0f);
    f.context.begin_frame(pointer(0.0f, 0.0f));
    {
        ui::BoxScope box("scoped", style);
    }
    CHECK_NOTHROW(f.context.end_frame());
    Rect rect;
    REQUIRE(f.context.layout_rect(f.context.id("scoped"), rect));
    CHECK(rect.size == Vec2f(40.0f, 20.0f));
}

TEST_CASE("UI: functions need an active UiContext and each context type has its own slot")
{
    struct OtherContext : ImContext
    {
    };
    {
        UiFixture f;
        CHECK(ActiveContext<UiContext>::get() == &f.context);
        CHECK(ActiveContext<OtherContext>::get() == nullptr);
    }
    CHECK(ActiveContext<UiContext>::get() == nullptr);
    CHECK_THROWS_AS(ui::label("AB"), Error);
    CHECK_THROWS_AS(ui::status_line("AB"), Error);
}

TEST_CASE("UI: ui::item reports the pointer for custom widgets")
{
    UiFixture f;
    const Rect area = { { 20.0f, 20.0f }, { 30.0f, 30.0f } };
    ItemState state;
    f.frame(pointer(25.0f, 25.0f, true, true), [&] { state = ui::item(ui::id("custom"), area); });
    CHECK(state.hovered);
    CHECK(state.pressed);
    f.frame(pointer(25.0f, 25.0f, false, false, true), [&] { state = ui::item(ui::id("custom"), area); });
    CHECK(state.clicked);
}

TEST_CASE("UI: a warm frame of widgets allocates nothing")
{
    UiFixture f;
    bool value = false;
    const auto frame = [&]
    {
        f.frame(pointer(10.0f, 10.0f), [&]
        {
            ui::PanelScope panel("panel");
            ui::label("AB");
            std::ignore = ui::button("ABC");
            ui::toggle("DE", value);
            ui::status_line("AB");
        });
    };
    frame();
    frame();
    MemoryStats before = test::all_allocations();
    frame();
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}

TEST_CASE("ui: row and column equal the hand-written boxes")
{
    UiFixture sugar;
    sugar.frame(pointer(0.0f, 0.0f), [&] {
        ui::RowOptions row;
        row.gap = 6.0f;
        row.padding = uniform_insets(4.0f);
        row.align = Align::Centre;
        row.height = fixed(40.0f);
        ui::RowScope scope("row", row);
        ui::label("a");
        ui::spacer(2.0f);
        ui::ColumnScope inner("col", { fixed(50.0f), fit(), {}, 2.0f, Align::End });
        ui::label("b");
    });

    UiFixture manual;
    manual.frame(pointer(0.0f, 0.0f), [&] {
        LayoutStyle row;
        row.direction = Direction::Row;
        row.width = fit();
        row.height = fixed(40.0f);
        row.padding = uniform_insets(4.0f);
        row.gap = 6.0f;
        row.align_y = Align::Centre;
        ui::BoxScope row_scope("row", row);
        manual.context.push_id("row");
        ui::label("a");
        LayoutStyle spacer;
        spacer.width = grow(2.0f);
        spacer.height = grow(2.0f);
        manual.context.begin_box(ImId{}, spacer);
        manual.context.end_box();
        LayoutStyle column;
        column.direction = Direction::Column;
        column.width = fixed(50.0f);
        column.gap = 2.0f;
        column.align_x = Align::End;
        {
            ui::BoxScope column_scope("col", column);
            manual.context.push_id("col");
            ui::label("b");
            manual.context.pop_id();
        }
        manual.context.pop_id();
    });
    CHECK(dump_layout(sugar.context) == dump_layout(manual.context));
    CHECK(dump(sugar.context.draw_list()) == dump(manual.context.draw_list()));
}

TEST_CASE("ui: a separator runs across a column and down a row")
{
    UiFixture fixture;
    fixture.frame(pointer(0.0f, 0.0f), [&] {
        ui::ColumnScope column("column", { fixed(80.0f), fit() });
        ui::separator();
        ui::RowScope row("row", { fit(), fixed(30.0f) });
        ui::separator();
    });
    const std::string layout = dump_layout(fixture.context);
    CHECK(layout.find("w=grow(1.00) h=fixed(1.00)") != std::string::npos);
    CHECK(layout.find("w=fixed(1.00) h=grow(1.00)") != std::string::npos);
}

TEST_CASE("ui: an image and an image button use the shared widgets")
{
    UiFixture f;
    uint32_t clicks = 0;
    const auto body = [&]
    {
        std::ignore = ui::image("pic", k_single_image);
        clicks += ui::image_button("button", ImageHandle{ 1 }).clicked ? 1 : 0;
    };
    for (uint32_t index = 0; index < 3; ++index)
    {
        f.frame(pointer(500.0f, 500.0f), body);
    }
    REQUIRE(f.context.draw_list().channel(0).images.size() == 2);
    CHECK(f.context.draw_list().channel(0).images[1].image.index == 1);
    const Vec2f at = rect_centre(f.context.draw_list().channel(0).images[1].rect);
    f.frame(pointer(at[0], at[1]), body);
    f.frame(pointer(at[0], at[1], true, true), body);
    f.frame(pointer(at[0], at[1], false, false, true), body);
    CHECK(clicks == 1);
}
