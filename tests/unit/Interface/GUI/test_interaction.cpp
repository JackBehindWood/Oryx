#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/support/GuiFixture.h"

using namespace oryx;
using test::GuiFixture;

namespace
{

// Boxes inside the field named `name` that paint a fill and no text, in tree order.
uint32_t filled_boxes_in(const GuiFixture& f, std::string_view name, const LayoutNode** first = nullptr)
{
    Rect row;
    if (!f.context.layout_rect(f.context.id(name), row))
    {
        return 0;
    }
    uint32_t count = 0;
    for (uint32_t index = 0; index < f.context.layout().node_count(); ++index)
    {
        const LayoutNode& node = f.context.layout().node(index);
        if (node.paint.has_fill && node.paint.text.empty() && contains(row, node.rect.min) && node.parent >= 0)
        {
            if (count++ == 0 && first != nullptr)
            {
                *first = &node;
            }
        }
    }
    return count;
}

bool same_colour(const Colour& a, const Colour& b)
{
    return math::abs(a.r - b.r) < 0.001f && math::abs(a.g - b.g) < 0.001f && math::abs(a.b - b.b) < 0.001f;
}

const LayoutNode* box_of(const GuiFixture& f, std::string_view name)
{
    const LayoutNode* mark = nullptr;
    std::ignore = filled_boxes_in(f, name, &mark);
    return mark;
}

} // namespace

TEST_CASE("GUI interaction: a checkbox ignores clicks on empty row space")
{
    GuiFixture f;
    bool value = false;
    const auto body = [&] { std::ignore = gui::checkbox("flag", value); };
    f.driver.settle(f.column_of(body));
    Rect row;
    REQUIRE(f.context.layout_rect(f.context.id("flag"), row));
    f.driver.click({ row.min[0] + row.size[0] + 60.0f, rect_centre(row)[1] }, f.column_of(body));
    CHECK_FALSE(value);
}

TEST_CASE("GUI interaction: a checkbox toggles from its label and from its box")
{
    GuiFixture f;
    bool value = false;
    const auto body = [&] { std::ignore = gui::checkbox("flag", value); };
    f.driver.settle(f.column_of(body));
    const LayoutNode* label = f.find_text("flag");
    REQUIRE(label != nullptr);
    f.driver.click(rect_centre(label->rect), f.column_of(body));
    CHECK(value);
    const LayoutNode* box = box_of(f, "flag");
    REQUIRE(box != nullptr);
    f.driver.click(rect_centre(box->rect), f.column_of(body));
    CHECK_FALSE(value);
}

TEST_CASE("GUI interaction: a checked checkbox draws a check mark and an unchecked one does not")
{
    GuiFixture f;
    bool on = true;
    bool off = false;
    const auto body = [&]
    {
        std::ignore = gui::checkbox("on", on);
        std::ignore = gui::checkbox("off", off);
    };
    f.driver.settle(f.column_of(body));
    CHECK(f.find_icon(Icon::Check, 0) != nullptr);
    CHECK(f.find_icon(Icon::Check, 1) == nullptr);
}

TEST_CASE("GUI interaction: hovering a checked checkbox keeps it looking checked")
{
    GuiFixture f;
    bool value = true;
    const auto body = [&] { std::ignore = gui::checkbox("flag", value); };
    f.driver.settle(f.column_of(body));
    const Colour rest = box_of(f, "flag")->paint.fill;
    f.driver.move_to(rect_centre(box_of(f, "flag")->rect));
    f.driver.frame(f.column_of(body));
    f.driver.frame(f.column_of(body));
    const Colour hovered = box_of(f, "flag")->paint.fill;
    CHECK_FALSE(same_colour(hovered, f.theme.base.hover));
    CHECK(f.find_icon(Icon::Check) != nullptr);
    CHECK(math::abs(hovered.r - rest.r) + math::abs(hovered.g - rest.g) + math::abs(hovered.b - rest.b) < 0.25f);
}

TEST_CASE("GUI interaction: hovering an unchecked checkbox changes its look visibly")
{
    GuiFixture f;
    bool value = false;
    const auto body = [&] { std::ignore = gui::checkbox("flag", value); };
    f.driver.settle(f.column_of(body));
    const Colour rest = box_of(f, "flag")->paint.fill;
    f.driver.move_to(rect_centre(box_of(f, "flag")->rect));
    f.driver.frame(f.column_of(body));
    f.driver.frame(f.column_of(body));
    const Colour hovered = box_of(f, "flag")->paint.fill;
    CHECK(math::abs(hovered.r - rest.r) + math::abs(hovered.g - rest.g) + math::abs(hovered.b - rest.b) > 0.12f);
}

TEST_CASE("GUI interaction: a selected radio is a round ring with a dot, an unselected one has no dot")
{
    GuiFixture f;
    int32_t choice = 1;
    const auto body = [&]
    {
        std::ignore = gui::radio("one", choice, 1);
        std::ignore = gui::radio("two", choice, 2);
    };
    f.driver.settle(f.column_of(body));
    const LayoutNode* ring = box_of(f, "one");
    REQUIRE(ring != nullptr);
    CHECK(ring->paint.radius.top_left >= ring->rect.size[0] * 0.5f - 0.01f);
    CHECK(filled_boxes_in(f, "one") >= 2);
    CHECK(filled_boxes_in(f, "two") == 1);
}

TEST_CASE("GUI interaction: a clicked slider shows a focus ring")
{
    GuiFixture f;
    float value = 0.5f;
    const auto body = [&] { std::ignore = gui::slider_float("level", value, 0.0f, 1.0f); };
    f.driver.settle(f.column_of(body));
    const LayoutNode* track = nullptr;
    for (uint32_t index = 0; index < f.context.layout().node_count(); ++index)
    {
        const LayoutNode& node = f.context.layout().node(index);
        if (node.paint.has_fill && node.paint.border_width > 0.0f && node.rect.size[0] > 40.0f)
        {
            track = &node;
            break;
        }
    }
    REQUIRE(track != nullptr);
    CHECK_FALSE(same_colour(track->paint.border, f.theme.base.accent));
    f.driver.click(rect_centre(track->rect), f.column_of(body));
    f.driver.frame(f.column_of(body));
    track = nullptr;
    for (uint32_t index = 0; index < f.context.layout().node_count(); ++index)
    {
        const LayoutNode& node = f.context.layout().node(index);
        if (node.paint.has_fill && node.paint.border_width > 0.0f && node.rect.size[0] > 40.0f)
        {
            track = &node;
            break;
        }
    }
    REQUIRE(track != nullptr);
    CHECK(same_colour(track->paint.border, f.theme.base.accent));
}
