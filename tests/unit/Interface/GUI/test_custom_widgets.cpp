#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"

// The authoring guide: widgets of your own, written with nothing but the public gui:: functions the built-ins use.
// 1. A gizmo: an item with an explicit rect, a drag, and painting through a Painter.
// 2. A stateful widget: gui::state<T> under an id of the current scope.
// 3. A plot decoration: ordinary drawing between the calls of a PlotScope, in the plot's own coordinates.
// 4. A menu item: a widget frame whose click asks the open menus to close.
// 5. A view: what a Dashboard view looks like in this API.

using namespace oryx;
using test::GuiFixture;

namespace
{

// 1. A square handle at `position` that follows the pointer while dragged; true while it moves.
bool drag_handle(std::string_view name, Vec2f& position, Painter& painter)
{
    const GuiId id = gui::id(name);
    const Rect box = { position - Vec2f(6.0f, 6.0f), { 12.0f, 12.0f } };
    const ItemState item = gui::item(id, box);
    const ItemDrag drag = gui::item_drag(id);
    position = position + drag.delta;
    painter.fill_rect(box, item.hovered || drag.dragging ? Colour{ 1.0f, 1.0f, 1.0f, 1.0f } : Colour{ 0.6f, 0.6f, 0.6f, 1.0f });
    return drag.dragging;
}

struct Counter
{
    int32_t clicks = 0;
};

// 2. A button that counts its own clicks; the count lives in the context, not in the caller.
int32_t click_counter(std::string_view name)
{
    LayoutStyle frame;
    frame.padding = uniform_insets(4.0f);
    const ItemState item = gui::begin_widget(name, frame);
    Counter& counter = gui::state<Counter>(gui::id("count"));
    counter.clicks += item.clicked ? 1 : 0;
    const int32_t clicks = counter.clicks;
    gui::label(gui::context().arena().format("%s: %d", std::string(name).c_str(), clicks));
    gui::end_widget();
    return clicks;
}

// 3. A threshold band and the selected sample, drawn in the plot's coordinates.
void mark_plot(gui::PlotScope& plot, float threshold, uint32_t selected)
{
    const PlotArea area = plot.area();
    const Vec2f top = to_screen(area, area.x_min, area.y_max);
    const Vec2f bar = to_screen(area, area.x_max, threshold);
    plot.painter().fill_rect({ top, { bar[0] - top[0], bar[1] - top[1] } }, { 1.0f, 0.2f, 0.2f, 0.25f });
    plot.vline(static_cast<float>(selected), { 1.0f, 1.0f, 1.0f, 1.0f });
}

// 4. A menu row with a hand-drawn tick that closes the menus like a built-in item.
bool check_menu_item(std::string_view label, bool& value)
{
    LayoutStyle row;
    row.gap = 6.0f;
    row.width = grow();
    const ItemState item = gui::begin_widget(label, row);
    gui::label(value ? "[x]" : "[ ]");
    gui::label(label);
    gui::end_widget();
    if (item.clicked)
    {
        value = !value;
        gui::context().request_menu_close();
    }
    return item.clicked;
}

gui::PlotOptions plot_box()
{
    gui::PlotOptions options;
    options.width = fixed(100.0f);
    options.height = fixed(50.0f);
    return options;
}

} // namespace

TEST_CASE("custom widgets: a gizmo follows the pointer while it is dragged")
{
    GuiFixture f;
    Vec2f position{ 40.0f, 30.0f };
    bool moving = false;
    const auto build = [&]
    {
        im::CanvasArea area = gui::canvas("scene");
        moving = drag_handle("handle", position, area.painter);
    };
    f.driver.settle(f.frame_of(build));
    f.driver.drag({ 40.0f, 30.0f }, { 70.0f, 50.0f }, 5, f.frame_of(build));
    CHECK(position[0] == doctest::Approx(70.0f));
    CHECK(position[1] == doctest::Approx(50.0f));
    CHECK_FALSE(moving);
}

TEST_CASE("custom widgets: a stateful widget keeps its count between frames and drops it when unused")
{
    GuiFixture f;
    int32_t clicks = 0;
    bool shown = true;
    const auto build = [&] { clicks = shown ? click_counter("hits") : clicks; };
    f.driver.settle(f.column_of(build));
    f.driver.click(rect_centre(f.find_text("hits: 0")->rect), f.column_of(build));
    f.driver.click(rect_centre(f.find_text("hits: 1")->rect), f.column_of(build));
    CHECK(clicks == 2);
    shown = false;
    f.driver.run_frames(3, f.column_of(build));
    shown = true;
    f.driver.run_frames(2, f.column_of(build));
    CHECK(clicks == 0);
}

TEST_CASE("custom widgets: a plot decoration draws in the plot's coordinates")
{
    GuiFixture f;
    f.driver.input().surface_size = { 200.0f, 100.0f };
    const float data[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    const auto build = [&]
    {
        gui::PlotScope plot("p", plot_box());
        plot.line("ramp", values(data));
        mark_plot(plot, 6.0f, 4);
    };
    f.driver.settle(f.frame_of(build));
    f.driver.run_frames(2, f.frame_of(build));
    bool band = false;
    for (const RectCmd& command : f.context.draw_list().channel(0).rects)
    {
        band = band || (command.colour.r == doctest::Approx(1.0f) && command.colour.a == doctest::Approx(0.25f));
    }
    CHECK(band);
    CHECK(f.context.draw_list().channel(0).lines.size() >= 10);
}

TEST_CASE("custom widgets: a menu item of your own fires and closes the menus")
{
    GuiFixture f;
    f.driver.input().surface_size = { 240.0f, 200.0f };
    bool ticked = false;
    const auto build = [&]
    {
        gui::MenuBarScope bar("bar");
        gui::MenuScope tools("Tools");
        if (tools.open())
        {
            std::ignore = check_menu_item("Snap", ticked);
        }
    };
    f.driver.settle(f.column_of(build));
    f.driver.click(rect_centre(f.find_text("Tools")->rect), f.column_of(build));
    f.driver.settle(f.column_of(build));
    REQUIRE(f.find_text("Snap") != nullptr);
    f.driver.click(rect_centre(f.find_text("Snap")->rect), f.column_of(build));
    f.driver.run_frames(3, f.column_of(build));
    CHECK(ticked);
    CHECK(f.find_text("Snap") == nullptr);
}

namespace
{

struct Record
{
    uint32_t ply;
    float value;
    uint32_t action;
};

struct ViewData
{
    const float* policy;
    uint32_t actions;
    const Record* trace;
    uint32_t trace_count;
    uint32_t selected_ply;
};

class IView
{
public:
    virtual ~IView() = default;
    virtual void draw(const ViewData& data) = 0;
};

// 5. A whole view: the policy as bars, the value history with the selected ply, and the trace as a table.
class SearchView final : public IView
{
public:
    void draw(const ViewData& data) override
    {
        FrameArena& arena = gui::context().arena();
        for (uint32_t action = 0; action < data.actions; ++action)
        {
            gui::bar(arena.format("action %u", action), data.policy[action], 1.0f, { .format = { NumberStyle::Percent, 0 } });
        }
        {
            gui::PlotScope plot("value", plot_box());
            plot.line("value", values_strided(&data.trace[0].value, data.trace_count, sizeof(Record)));
            plot.vline(static_cast<float>(data.selected_ply), gui::theme().base.accent);
        }
        gui::TableScope table("trace", { .row_count = data.trace_count, .height = fixed(90.0f) });
        table.column("Ply", fixed(40.0f));
        table.column("Action", grow());
        std::ignore = table.headers();
        for (uint32_t row = table.first_row(); row < table.last_row(); ++row)
        {
            std::ignore = table.row(row, data.trace[row].ply == data.selected_ply);
            table.cell(arena.format("%u", data.trace[row].ply));
            table.cell(arena.format("action %u", data.trace[row].action));
        }
    }
};

} // namespace

TEST_CASE("custom widgets: a dashboard-style view is a short function over the data")
{
    GuiFixture f;
    f.driver.input().surface_size = { 300.0f, 400.0f };
    const float policy[3] = { 0.5f, 0.3f, 0.2f };
    const Record trace[4] = { { 1, 0.0f, 2 }, { 2, 0.4f, 0 }, { 3, -0.2f, 1 }, { 4, 0.9f, 0 } };
    SearchView view;
    const auto build = [&] { view.draw({ policy, 3, trace, 4, 3 }); };
    f.driver.settle(f.column_of(build));
    f.driver.run_frames(2, f.column_of(build));
    CHECK(f.find_text("action 0") != nullptr);
    CHECK(f.find_text("50%") != nullptr);
    CHECK(f.find_text("Ply") != nullptr);
    CHECK(f.context.draw_list().channel(0).lines.size() >= 4);

    const MemoryStats before = test::all_allocations();
    f.driver.run_frames(3, f.column_of(build));
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}

TEST_CASE("GUI review: the theme is versioned plain data and a high-contrast variant is a named style")
{
    static_assert(std::is_trivially_copyable_v<GuiTheme> && std::is_trivially_copyable_v<LayoutStyle>);
    GuiFixture f;
    CHECK(f.theme.version == 1);
    GuiTheme copy = f.theme;
    ImStyle contrast = f.theme.base;
    contrast.background = { 0.0f, 0.0f, 0.0f, 1.0f };
    contrast.text = { 1.0f, 1.0f, 0.0f, 1.0f };
    contrast.border = { 1.0f, 1.0f, 1.0f, 1.0f };
    contrast.border_width = 2.0f;
    add_style_variant(copy, "high_contrast", contrast);
    f.context.set_theme(copy);
    const auto build = [&] { std::ignore = gui::button("Run", { .variant = "high_contrast" }); };
    f.driver.settle(f.frame_of(build));
    bool found = false;
    for (const RoundedRectCmd& command : f.context.draw_list().channel(0).rounded_rects)
    {
        found = found || approx_equal(command.colour, contrast.background);
    }
    CHECK(found);
    REQUIRE(f.find_text("Run") != nullptr);
    CHECK(f.context.draw_list().channel(0).texts.size() == 1);
    CHECK(approx_equal(f.context.draw_list().channel(0).texts[0].colour, contrast.text));
}

TEST_CASE("GUI review: the surface id of the input tags the recorded list")
{
    GuiFixture f;
    f.driver.input().surface = 1;
    f.driver.frame(f.frame_of([] {}));
    CHECK(f.context.draw_list().surface() == 1);
    f.driver.input().surface = 2;
    f.driver.frame(f.frame_of([] {}));
    CHECK(f.context.draw_list().surface() == 2);
}
