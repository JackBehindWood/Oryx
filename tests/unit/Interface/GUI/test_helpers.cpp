#include "doctest.h"

#include "Oryx.h"
#include "Oryx/Interface/GUI/GuiEdit.h"
#include "Oryx/Interface/GUI/GuiRows.h"
#include "unit/Interface/support/GuiFixture.h"

using namespace oryx;
using test::GuiFixture;

namespace
{

enum class Mode : uint8_t
{
    Heat,
    Chosen,
    Value
};

const char* const k_modes[3] = { "heat", "chosen", "value" };

// The draw list of three frames of `body`, so layout has settled when it is read.
template<typename Body>
std::string settled_dump(Body&& body)
{
    GuiFixture f;
    f.driver.input().surface_size = { 300.0f, 200.0f };
    f.driver.run_frames(3, f.column_of(body));
    return dump(f.context.draw_list());
}

}

TEST_CASE("edit helpers: each overload draws what the control it dispatches to draws")
{
    bool flag = true;
    int32_t count = 7;
    float ratio = 0.5f;
    const std::string helper = settled_dump(
        [&]
        {
            std::ignore = gui::edit("flag", flag);
            std::ignore = gui::edit("count", count);
            std::ignore = gui::edit("ratio", ratio);
        });
    const std::string manual = settled_dump(
        [&]
        {
            std::ignore = gui::checkbox("flag", flag);
            std::ignore = gui::input_int("count", count);
            std::ignore = gui::input_float("ratio", ratio);
        });
    CHECK(helper == manual);
    CHECK_FALSE(helper.empty());
}

TEST_CASE("edit helpers: a Vec2f is two number fields in one labelled row")
{
    Vec2f point{ 1.0f, 2.0f };
    const std::string helper = settled_dump([&] { std::ignore = gui::edit("point", point); });
    const std::string manual = settled_dump(
        [&]
        {
            gui::FieldScope field("point", {});
            gui::NumberOptions each;
            each.label_width = 0.0f;
            std::ignore = gui::input_float("##x", point[0], each);
            std::ignore = gui::input_float("##y", point[1], each);
        });
    CHECK(helper == manual);
}

TEST_CASE("edit helpers: a char array and an enum edit through text_input and a combo")
{
    char name[16] = "tic";
    Mode mode = Mode::Chosen;
    const std::string helper = settled_dump(
        [&]
        {
            std::ignore = gui::edit("name", name);
            std::ignore = gui::edit("mode", mode, k_modes, 3);
        });
    const std::string manual = settled_dump(
        [&]
        {
            std::ignore = gui::text_input("name", name, 16);
            if (gui::ComboScope combo("mode", "chosen"); combo)
            {
                for (uint32_t index = 0; index < 3; ++index)
                    std::ignore = combo.item(k_modes[index], index == 1);
            }
        });
    CHECK(helper == manual);
}

TEST_CASE("edit helpers: an enum pick returns true and stores the picked value")
{
    GuiFixture f;
    f.driver.input().surface_size = { 300.0f, 200.0f };
    Mode mode = Mode::Heat;
    bool changed = false;
    const auto body = [&] { changed = gui::edit("mode", mode, k_modes, 3) || changed; };
    f.driver.run_frames(3, f.column_of(body));
    const LayoutNode* header = f.find_text("heat");
    REQUIRE(header != nullptr);
    f.driver.move_to(header->rect.min + header->rect.size * 0.5f);
    f.driver.press();
    f.driver.run_frames(2, f.column_of(body));
    f.driver.release();
    f.driver.run_frames(3, f.column_of(body));
    const LayoutNode* item = f.find_text("value");
    REQUIRE(item != nullptr);
    f.driver.move_to(item->rect.min + item->rect.size * 0.5f);
    f.driver.run_frames(2, f.column_of(body));
    f.driver.press();
    f.driver.frame(f.column_of(body));
    f.driver.release();
    f.driver.run_frames(2, f.column_of(body));
    CHECK(changed);
    CHECK(mode == Mode::Value);
}

TEST_CASE("row helpers: range-for submits the same rows as the manual loop")
{
    const auto table_body = [](bool ranged)
    {
        return [ranged]
        {
            gui::TableOptions options;
            options.row_count = 500;
            options.row_height = 20.0f;
            options.height = fixed(100.0f);
            gui::TableScope table("rows", options);
            table.column("n", fixed(40.0f));
            std::ignore = table.headers();
            if (ranged)
            {
                for (uint32_t row : gui::table_rows())
                {
                    std::ignore = table.row(row);
                    table.cell("x");
                }
            }
            else
            {
                for (uint32_t row = table.first_row(); row < table.last_row(); ++row)
                {
                    std::ignore = table.row(row);
                    table.cell("x");
                }
            }
        };
    };
    const std::string helper = settled_dump(table_body(true));
    const std::string manual = settled_dump(table_body(false));
    CHECK(helper == manual);
    CHECK_FALSE(helper.empty());
}

TEST_CASE("row helpers: list_items matches first_item to last_item and an empty range is empty")
{
    const auto list_body = [](bool ranged)
    {
        return [ranged]
        {
            gui::ListBoxOptions options;
            options.item_count = 300;
            gui::ListBoxScope list("items", options);
            if (ranged)
            {
                for (uint32_t index : gui::list_items())
                    std::ignore = list.item(index, "item", false);
            }
            else
            {
                for (uint32_t index = list.first_item(); index < list.last_item(); ++index)
                    std::ignore = list.item(index, "item", false);
            }
        };
    };
    CHECK(settled_dump(list_body(true)) == settled_dump(list_body(false)));
    const gui::IndexRange none(5, 3);
    CHECK(none.size() == 0);
    uint32_t visited = 0;
    for (uint32_t index : gui::IndexRange(2, 5))
        visited += index;
    CHECK(visited == 2 + 3 + 4);
}
