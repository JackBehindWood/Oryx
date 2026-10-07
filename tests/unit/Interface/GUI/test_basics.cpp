#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;
using test::GuiFixture;

namespace
{

// The first filled box without text in the field named `name`, i.e. its control mark.
const LayoutNode* mark_of(const GuiFixture& f, std::string_view name)
{
    Rect row;
    if (!f.context.layout_rect(f.context.id(name), row))
    {
        return nullptr;
    }
    for (uint32_t index = 0; index < f.context.layout().node_count(); ++index)
    {
        const LayoutNode& node = f.context.layout().node(index);
        if (node.paint.has_fill && node.paint.text.empty() && contains(row, node.rect.min) && node.parent >= 0 && f.context.layout().node(static_cast<uint32_t>(node.parent)).id == f.context.id(name))
        {
            return &node;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("GUI basics: a checkbox flips on a click anywhere on its row")
{
    GuiFixture f;
    bool value = false;
    uint32_t changes = 0;
    const auto body = [&] { changes += gui::checkbox("flag", value) ? 1 : 0; };
    f.driver.settle(f.column_of(body));
    const LayoutNode* label = f.find_text("flag");
    REQUIRE(label != nullptr);
    f.driver.click(rect_centre(label->rect), f.column_of(body));
    CHECK(value);
    CHECK(changes == 1);
    f.driver.click(rect_centre(label->rect), f.column_of(body));
    CHECK_FALSE(value);
}

TEST_CASE("GUI basics: the label side comes from the theme and the call can override it")
{
    GuiFixture f;
    bool value = false;
    const auto body = [&] { std::ignore = gui::checkbox("flag", value); };
    f.driver.settle(f.column_of(body));
    const LayoutNode* label = f.find_text("flag");
    const LayoutNode* mark = mark_of(f, "flag");
    REQUIRE(label != nullptr);
    REQUIRE(mark != nullptr);
    CHECK(label->rect.min[0] < mark->rect.min[0]);

    const auto after = [&] { std::ignore = gui::checkbox("flag", value, { .label_side = LabelSide::After }); };
    f.driver.settle(f.column_of(after));
    label = f.find_text("flag");
    mark = mark_of(f, "flag");
    REQUIRE(label != nullptr);
    REQUIRE(mark != nullptr);
    CHECK(label->rect.min[0] > mark->rect.min[0]);

    f.theme.label_side = LabelSide::After;
    f.context.set_theme(f.theme);
    f.driver.settle(f.column_of(body));
    CHECK(f.find_text("flag")->rect.min[0] > mark_of(f, "flag")->rect.min[0]);
}

TEST_CASE("GUI basics: a label width aligns the controls of stacked fields")
{
    GuiFixture f;
    bool a = false;
    bool b = false;
    const auto body = [&]
    {
        std::ignore = gui::checkbox("a", a, { .label_width = 60.0f });
        std::ignore = gui::checkbox("a long label", b, { .label_width = 60.0f });
    };
    f.driver.settle(f.column_of(body));
    CHECK(mark_of(f, "a")->rect.min[0] == doctest::Approx(mark_of(f, "a long label")->rect.min[0]));
}

TEST_CASE("GUI basics: radio sets its option, selectable reports the click")
{
    GuiFixture f;
    int32_t choice = 0;
    uint32_t clicks = 0;
    const auto body = [&]
    {
        std::ignore = gui::radio("one", choice, 1);
        std::ignore = gui::radio("two", choice, 2);
        clicks += gui::selectable("row", false).clicked ? 1 : 0;
    };
    f.driver.settle(f.column_of(body));
    f.driver.click(rect_centre(f.find_text("two")->rect), f.column_of(body));
    CHECK(choice == 2);
    f.driver.click(rect_centre(f.find_text("row")->rect), f.column_of(body));
    CHECK(clicks == 1);
}

TEST_CASE("GUI basics: progress clamps the fraction")
{
    GuiFixture f;
    float fraction = 2.0f;
    const auto body = [&] { gui::progress("load", fraction); };
    f.driver.settle(f.column_of(body));
    const Colour accent = f.theme.base.accent;
    const auto fill_width = [&]
    {
        for (uint32_t index = 0; index < f.context.layout().node_count(); ++index)
        {
            const LayoutNode& node = f.context.layout().node(index);
            if (node.paint.has_fill && approx_equal(node.paint.fill, accent))
            {
                return node.rect.size[0];
            }
        }
        return -1.0f;
    };
    const float full = fill_width();
    CHECK(full > 10.0f);
    fraction = -1.0f;
    f.driver.settle(f.column_of(body));
    CHECK(fill_width() == doctest::Approx(0.0f));
    fraction = 0.5f;
    f.driver.settle(f.column_of(body));
    CHECK(fill_width() == doctest::Approx(full * 0.5f));
}

TEST_CASE("GUI basics: key_value pushes the value to the right edge")
{
    GuiFixture f;
    const auto body = [&] { gui::key_value("nodes", "1204"); };
    f.driver.settle(f.column_of(body));
    Rect row;
    REQUIRE(f.context.layout_rect(f.context.id("nodes"), row));
    const LayoutNode* value = f.find_text("1204");
    REQUIRE(value != nullptr);
    CHECK(rect_max(value->rect)[0] == doctest::Approx(rect_max(row)[0]));
}

TEST_CASE("GUI basics: text_coloured, bullet, badge, swatch and small_button build")
{
    GuiFixture f;
    uint32_t clicks = 0;
    const auto body = [&]
    {
        gui::text_coloured("warn", { 1.0f, 0.0f, 0.0f, 1.0f });
        gui::bullet("point");
        gui::badge("new", { 0.0f, 1.0f, 0.0f, 1.0f });
        gui::colour_swatch("tint", { 0.0f, 0.0f, 1.0f, 1.0f });
        clicks += gui::small_button("go").clicked ? 1 : 0;
    };
    f.driver.settle(f.column_of(body));
    CHECK(f.find_text("warn")->paint.text_colour.r == doctest::Approx(1.0f));
    f.driver.click(rect_centre(f.find_text("go")->rect), f.column_of(body));
    CHECK(clicks == 1);
}

TEST_CASE("GUI basics: warm frames allocate nothing")
{
    GuiFixture f;
    bool flag = false;
    int32_t choice = 1;
    const auto body = [&]
    {
        std::ignore = gui::checkbox("flag", flag);
        std::ignore = gui::radio("one", choice, 1);
        std::ignore = gui::selectable("row", false);
        gui::progress("load", 0.5f, "50%");
        gui::key_value("nodes", "1204");
        gui::bullet("point");
        gui::badge("new", { 0.0f, 1.0f, 0.0f, 1.0f });
        gui::text_coloured("warn", { 1.0f, 0.0f, 0.0f, 1.0f });
    };
    f.driver.run_frames(4, f.column_of(body));
    const MemoryStats before = test::all_allocations();
    f.driver.run_frames(3, f.column_of(body));
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}
