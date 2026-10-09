#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/support/GuiFixture.h"

using namespace oryx;
using test::GuiFixture;

namespace
{

float distance(const Colour& a, const Colour& b)
{
    return math::abs(a.r - b.r) + math::abs(a.g - b.g) + math::abs(a.b - b.b) + math::abs(a.a - b.a);
}

void use_dark(GuiFixture& f)
{
    GuiTheme theme = dark_gui_theme();
    theme.font = &f.font;
    theme.min_hit_size = 0.0f;
    f.context.set_theme(theme);
}

// The first box without text whose fill is set, i.e. the mark of a checkbox or radio.
const LayoutNode* mark(const GuiFixture& f)
{
    for (uint32_t index = 0; index < f.context.layout().node_count(); ++index)
    {
        const LayoutNode& node = f.context.layout().node(index);
        if (node.paint.has_fill && node.paint.text.empty() && node.rect.size[0] > 0.0f && node.rect.size[0] <= 24.0f && node.paint.border_width > 0.0f)
        {
            return &node;
        }
    }
    return nullptr;
}

struct Fills
{
    Colour rest;
    Colour hover;
    Colour pressed;
};

// Fill of the box `find` returns while idle, hovered and held.
template<typename Build, typename Find>
Fills measure(GuiFixture& f, Build&& build, Find&& find)
{
    f.driver.settle(f.column_of(build));
    Fills fills;
    const LayoutNode* node = find(f);
    REQUIRE(node != nullptr);
    fills.rest = node->paint.fill;
    const Vec2f at = rect_centre(node->rect);
    f.driver.move_to(at);
    f.driver.run_frames(2, f.column_of(build));
    fills.hover = find(f)->paint.fill;
    f.driver.press();
    f.driver.frame(f.column_of(build));
    fills.pressed = find(f)->paint.fill;
    f.driver.release();
    f.driver.leave();
    f.driver.run_frames(2, f.column_of(build));
    return fills;
}

const LayoutNode* text_node(const GuiFixture& f, std::string_view text)
{
    return f.find_text(text);
}

} // namespace

TEST_CASE("themes: dark, light and high contrast keep text readable on every role")
{
    const GuiTheme themes[3] = { dark_gui_theme(), light_gui_theme(), high_contrast_gui_theme() };
    const float required[3] = { 4.5f, 4.5f, 7.0f };
    for (uint32_t index = 0; index < 3; ++index)
    {
        const GuiTheme& theme = themes[index];
        const ImStyle* roles[] = { &theme.base, &theme.panel, &theme.header, &theme.field, &theme.button, &theme.tab, &theme.overlay };
        for (const ImStyle* role : roles)
        {
            CHECK(contrast_ratio(role->text, role->background) >= required[index]);
            CHECK(contrast_ratio(role->text, role->selected) >= required[index]);
        }
        CHECK(contrast_ratio(theme.field.accent, theme.field.background) >= 3.0f);
        CHECK(contrast_ratio(theme.field.on_accent, theme.field.accent) >= required[index]);
    }
}

TEST_CASE("themes: the three looks differ, share the version and stay plain data")
{
    static_assert(std::is_trivially_copyable_v<GuiTheme>);
    const GuiTheme dark = dark_gui_theme();
    const GuiTheme light = light_gui_theme();
    const GuiTheme contrast = high_contrast_gui_theme();
    CHECK(dark.version == 2);
    CHECK(light.version == 2);
    CHECK(contrast.version == 2);
    CHECK(distance(dark.panel.background, light.panel.background) > 1.0f);
    CHECK(contrast.field.border_width >= 2.0f);
    CHECK(dark.field.border_width == doctest::Approx(1.0f));
}

TEST_CASE("themes: surfaces are neutral greys and the accent is its own colour")
{
    const GuiTheme dark = dark_gui_theme();
    const ImStyle* surfaces[] = { &dark.panel, &dark.header, &dark.field, &dark.button, &dark.overlay };
    for (const ImStyle* surface : surfaces)
    {
        CHECK(math::abs(surface->background.r - surface->background.g) < 0.02f);
        CHECK(math::abs(surface->background.g - surface->background.b) < 0.02f);
    }
    CHECK(distance(dark.field.accent, dark.field.background) > 0.8f);
}

TEST_CASE("themes: scaling multiplies the sizes and 1 undoes it")
{
    const GuiTheme theme = dark_gui_theme();
    const GuiTheme big = scale_gui_theme(theme, 2.0f);
    CHECK(big.field.text_height == doctest::Approx(theme.field.text_height * 2.0f));
    CHECK(big.field.padding.left == doctest::Approx(theme.field.padding.left * 2.0f));
    CHECK(big.spacing == doctest::Approx(theme.spacing * 2.0f));
    CHECK(big.scrollbar_width == doctest::Approx(theme.scrollbar_width * 2.0f));
    CHECK(distance(big.field.background, theme.field.background) == doctest::Approx(0.0f));
    const GuiTheme back = scale_gui_theme(big, 0.5f);
    CHECK(back.field.text_height == doctest::Approx(theme.field.text_height));
    CHECK(back.spacing == doctest::Approx(theme.spacing));
}

TEST_CASE("themes: a context switches between the three and back")
{
    GuiFixture f;
    const GuiTheme dark = dark_gui_theme();
    f.context.set_theme(light_gui_theme());
    CHECK(distance(f.context.gui_theme().panel.background, dark.panel.background) > 1.0f);
    f.context.set_theme(dark_gui_theme());
    CHECK(distance(f.context.gui_theme().panel.background, dark.panel.background) == doctest::Approx(0.0f));
}

TEST_CASE("states: a button tints on hover and again on press")
{
    GuiFixture f;
    use_dark(f);
    const auto body = [&] { std::ignore = gui::button("Run"); };
    const Fills fills = measure(f, body, [](const GuiFixture& g) { return text_node(g, "Run"); });
    CHECK(distance(fills.rest, fills.hover) > 0.05f);
    CHECK(distance(fills.hover, fills.pressed) > 0.05f);
    CHECK(f.context.output().cursor == CursorShape::Arrow);
}

TEST_CASE("states: a button asks for the hand cursor while hovered")
{
    GuiFixture f;
    use_dark(f);
    const auto body = [&] { std::ignore = gui::button("Run"); };
    f.driver.settle(f.column_of(body));
    f.driver.move_to(rect_centre(f.find_text("Run")->rect));
    f.driver.run_frames(2, f.column_of(body));
    CHECK(f.context.output().cursor == CursorShape::Hand);
}

TEST_CASE("states: an unchecked checkbox and a radio tint on hover and press")
{
    GuiFixture f;
    use_dark(f);
    bool flag = false;
    const auto check = [&] { std::ignore = gui::checkbox("flag", flag); };
    const Fills box = measure(f, check, [](const GuiFixture& g) { return mark(g); });
    CHECK(distance(box.rest, box.hover) > 0.03f);
    CHECK(distance(box.hover, box.pressed) > 0.03f);
    flag = false;
    GuiFixture g;
    use_dark(g);
    int32_t choice = 1;
    const auto radio = [&] { std::ignore = gui::radio("pick", choice, 0); };
    const Fills ring = measure(g, radio, [](const GuiFixture& h) { return mark(h); });
    CHECK(distance(ring.rest, ring.hover) > 0.03f);
}

TEST_CASE("states: a chosen selectable stays selected under the pointer and differs from an unchosen one")
{
    GuiFixture f;
    use_dark(f);
    const auto body = [&]
    {
        std::ignore = gui::selectable("chosen", true);
        std::ignore = gui::selectable("plain", false);
    };
    const Fills chosen = measure(f, body, [](const GuiFixture& g) { return text_node(g, "chosen"); });
    CHECK(distance(chosen.rest, chosen.hover) > 0.03f);
    CHECK(distance(chosen.hover, chosen.pressed) > 0.03f);
    CHECK(distance(chosen.hover, f.context.gui_theme().panel.hover) > 0.05f);
    const LayoutNode* plain = text_node(f, "plain");
    REQUIRE(plain != nullptr);
    CHECK_FALSE(plain->paint.has_fill);
}

TEST_CASE("states: a toggle button on versus off")
{
    GuiFixture f;
    use_dark(f);
    bool on = true;
    bool off = false;
    const auto body = [&]
    {
        std::ignore = gui::toggle("on", on);
        std::ignore = gui::toggle("off", off);
    };
    f.driver.settle(f.column_of(body));
    CHECK(distance(text_node(f, "on")->paint.fill, text_node(f, "off")->paint.fill) > 0.05f);
}

TEST_CASE("states: the selected tab has an accent underline and the others none")
{
    GuiFixture f;
    use_dark(f);
    uint32_t selected = 1;
    const auto body = [&]
    {
        gui::TabBarScope bar("tabs", selected);
        std::ignore = bar.tab("A");
        std::ignore = bar.tab("B");
    };
    f.driver.settle(f.column_of(body));
    uint32_t underlines = 0;
    for (uint32_t index = 0; index < f.context.layout().node_count(); ++index)
    {
        const LayoutNode& node = f.context.layout().node(index);
        underlines += node.paint.has_fill && node.style.height.kind == SizingKind::Fixed && node.style.height.value == 2.0f && distance(node.paint.fill, f.context.gui_theme().tab.accent) < 0.001f ? 1u : 0u;
    }
    CHECK(underlines == 1);
}

TEST_CASE("disabled: widgets ignore the pointer, report nothing and paint dimmed")
{
    GuiFixture f;
    use_dark(f);
    uint32_t clicks = 0;
    bool flag = false;
    bool off = false;
    const auto body = [&]
    {
        clicks += gui::button("live").clicked ? 1u : 0u;
        gui::DisabledScope scope(off);
        clicks += gui::button("dead").clicked ? 1u : 0u;
        std::ignore = gui::checkbox("flag", flag);
    };
    off = true;
    f.driver.settle(f.column_of(body));
    f.driver.click(rect_centre(f.find_text("dead")->rect), f.column_of(body));
    f.driver.click(rect_centre(f.find_text("flag")->rect), f.column_of(body));
    CHECK(clicks == 0);
    CHECK_FALSE(flag);
    CHECK(f.find_text("dead")->alpha == doctest::Approx(k_disabled_alpha));
    CHECK(f.find_text("live")->alpha == doctest::Approx(1.0f));
    f.driver.click(rect_centre(f.find_text("live")->rect), f.column_of(body));
    CHECK(clicks == 1);
    off = false;
    f.driver.settle(f.column_of(body));
    f.driver.click(rect_centre(f.find_text("flag")->rect), f.column_of(body));
    CHECK(flag);
}

TEST_CASE("disabled: the dimming reaches the draw list colours")
{
    GuiFixture f;
    use_dark(f);
    const auto body = [&]
    {
        gui::DisabledScope scope;
        std::ignore = gui::button("dead");
    };
    f.driver.settle(f.column_of(body));
    bool faded = false;
    const DrawList& list = f.context.draw_list();
    for (const auto& rect : list.channel(0).rounded_rects)
    {
        faded = faded || rect.colour.a < 1.0f;
    }
    for (const auto& rect : list.channel(0).rects)
    {
        faded = faded || rect.colour.a < 1.0f;
    }
    CHECK(faded);
}

TEST_CASE("disabled: scopes must balance")
{
    GuiFixture f;
    f.context.begin_frame(f.driver.input());
    f.context.begin_disabled();
    CHECK_THROWS_AS(f.context.end_frame(), Error);
    f.context.abort_frame();
    f.context.begin_frame(f.driver.input());
    CHECK_THROWS_AS(f.context.end_disabled(), Error);
    f.context.abort_frame();
}

TEST_CASE("selection: plain, toggle and range clicks")
{
    uint64_t words[selection_words(100)] = {};
    Selection selection;
    ImKeys plain;
    ImKeys toggle;
    toggle.shortcut = true;
    ImKeys range;
    range.shift = true;
    CHECK(select_click(selection, words, 100, 10, plain));
    CHECK(selection_size(words, 100) == 1);
    CHECK(select_click(selection, words, 100, 20, toggle));
    CHECK(selection_contains(words, 100, 10));
    CHECK(selection_contains(words, 100, 20));
    CHECK(select_click(selection, words, 100, 20, toggle));
    CHECK_FALSE(selection_contains(words, 100, 20));
    CHECK(select_click(selection, words, 100, 5, plain));
    CHECK(select_click(selection, words, 100, 8, range));
    CHECK(selection_size(words, 100) == 4);
    CHECK(selection_contains(words, 100, 5));
    CHECK(selection_contains(words, 100, 8));
    CHECK(select_click(selection, words, 100, 2, range));
    CHECK(selection_size(words, 100) == 4);
    CHECK(selection_contains(words, 100, 2));
    CHECK_FALSE(selection_contains(words, 100, 8));
    CHECK_FALSE(select_click(selection, words, 100, 500, plain));
}

TEST_CASE("selection: a shift click without an anchor acts like a plain click, select_all and move work")
{
    uint64_t words[selection_words(70)] = {};
    Selection selection;
    ImKeys range;
    range.shift = true;
    CHECK(select_click(selection, words, 70, 3, range));
    CHECK(selection_size(words, 70) == 1);
    select_all(selection, words, 70);
    CHECK(selection_size(words, 70) == 70);
    selection = {};
    selection_clear(words, 70);
    CHECK(select_move(selection, words, 70, 1, ImKeys{}));
    CHECK(selection_contains(words, 70, 0));
    CHECK(select_move(selection, words, 70, 2, ImKeys{}));
    CHECK(selection_contains(words, 70, 2));
    CHECK(selection_size(words, 70) == 1);
    CHECK(select_move(selection, words, 70, 1, range));
    CHECK(selection_size(words, 70) == 2);
    CHECK(select_move(selection, words, 70, -100, ImKeys{}));
    CHECK(selection_contains(words, 70, 0));
}

TEST_CASE("selection: a list box row press picks, shortcut toggles and shift extends across rows")
{
    GuiFixture f;
    use_dark(f);
    constexpr uint32_t k_count = 6;
    uint64_t words[selection_words(k_count)] = {};
    Selection selection;
    const auto body = [&]
    {
        gui::ListBoxScope list("items", { .item_count = k_count });
        for (uint32_t index = list.first_item(); index < list.last_item(); ++index)
        {
            std::ignore = gui::select_on_press(list.item(index, f.context.arena().format("row %u", index), selection_contains(words, k_count, index)), index, selection, words, k_count);
        }
    };
    f.driver.settle(f.column_of(body));
    f.driver.click(rect_centre(f.find_text("row 1")->rect), f.column_of(body));
    CHECK(selection_size(words, k_count) == 1);
    f.driver.hold_shortcut(true);
    f.driver.click(rect_centre(f.find_text("row 4")->rect), f.column_of(body));
    f.driver.hold_shortcut(false);
    CHECK(selection_size(words, k_count) == 2);
    f.driver.input().keys.shift = true;
    f.driver.click(rect_centre(f.find_text("row 3")->rect), f.column_of(body));
    f.driver.input().keys.shift = false;
    CHECK(selection_contains(words, k_count, 3));
    CHECK(selection_contains(words, k_count, 4));
}

TEST_CASE("theme: the presets are their palettes, and a palette round-trips through make_gui_theme")
{
    const GuiTheme dark = dark_gui_theme();
    const GuiPalette palette = dark_gui_palette();
    CHECK(dark.panel.background == palette.panel);
    CHECK(dark.field.border == palette.field_border);
    CHECK(dark.button.hover == shift_colour(palette.button, palette.hover_step));
    CHECK(dark.button.pressed == shift_colour(palette.button, -palette.hover_step * 0.9f));
    CHECK(dark.tab.selected == palette.panel);
    CHECK(dark.base.border_width == 0.0f);
    CHECK(high_contrast_gui_theme().panel.border_width == 2.0f);
    CHECK(light_gui_theme().field.background == colour_from_hex(0xFFFFFF));
}

TEST_CASE("theme: set_accent reaches every role, keeps the chosen tab joined to its panel and picks a readable mark")
{
    GuiTheme theme = dark_gui_theme();
    const Colour red = colour_from_hex(0xD03020);
    set_accent(theme, red);
    for (const ImStyle* style : { &theme.base, &theme.panel, &theme.header, &theme.field, &theme.button, &theme.tab, &theme.scroll, &theme.overlay })
    {
        CHECK(style->accent == red);
        CHECK(contrast_ratio(style->accent, style->on_accent) >= 3.0f);
    }
    CHECK(theme.tab.selected == theme.panel.background);
    CHECK(theme.field.background == dark_gui_theme().field.background);
    CHECK(on_colour(colour_from_hex(0xFFD400)) == colour_from_hex(0x000000));
    CHECK(on_colour(colour_from_hex(0x101010)) == colour_from_hex(0xFFFFFF));
}

TEST_CASE("theme: size setters and apply_gui_palette keep what they are not about")
{
    GuiTheme theme = scale_gui_theme(dark_gui_theme(), 1.5f);
    theme.spacing = 11.0f;
    add_style_variant(theme, "danger", theme.button);
    set_text_height(theme, 20.0f);
    set_corner_radius(theme, 6.0f);
    CHECK(theme.field.text_height == 20.0f);
    CHECK(theme.overlay.radius == 6.0f);
    CHECK(theme.button.padding.left == doctest::Approx(12.0f));

    apply_gui_palette(theme, light_gui_palette());
    CHECK(theme.panel.background == light_gui_palette().panel);
    CHECK(theme.field.text_height == 20.0f);
    CHECK(theme.spacing == 11.0f);
    CHECK(theme.variant_count == 1);
}

TEST_CASE("theme: the gui:: setters edit the active theme and re-derive the dock theme")
{
    GuiFixture f;
    ContextScope<GuiContext> scope(f.context);
    const Colour green = colour_from_hex(0x30A040);
    gui::set_theme_accent(green);
    CHECK(gui::theme().panel.accent == green);
    CHECK(gui::dock_theme().preview_border == green);
    gui::set_theme_text_height(18.0f);
    CHECK(gui::theme().button.text_height == 18.0f);
    gui::GuiDockTheme dock = gui::dock_theme();
    dock.refusal = colour_from_hex(0xFF0000);
    gui::set_dock_theme(dock);
    CHECK(gui::dock_theme().refusal == colour_from_hex(0xFF0000));
    gui::set_theme(dark_gui_theme());
    CHECK(gui::dock_theme().refusal == gui::derive_dock_theme(dark_gui_theme()).refusal);
}
