#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/GUI/Dock/DockTestSupport.h"
#include "unit/Interface/support/GuiFixture.h"

using namespace oryx;
using namespace oryx::gui;
using namespace oryx::test;

namespace
{

constexpr const char* k_names[] = { "a", "b", "c", "d", "e", "f", "g", "h", "vp" };
constexpr uint32_t k_name_count = sizeof(k_names) / sizeof(k_names[0]);
constexpr float k_scales[] = { 1.0f, 1.5f, 2.0f };

PanelId random_panel(Lcg& rng) { return pid(k_names[rng.below(k_name_count)]); }

int32_t random_node(Lcg& rng, const DockLayout& layout, DockNodeKind kind)
{
    int32_t found = k_no_node;
    uint32_t seen = 0;
    for (uint32_t n = 0; n < layout.node_count; ++n)
        if (layout.nodes[n].kind == kind && rng.below(++seen) == 0)
            found = static_cast<int32_t>(n);
    return found;
}

DropZone random_zone(Lcg& rng) { return static_cast<DropZone>(rng.below(5)); }

Rect random_rect(Lcg& rng)
{
    return Rect{ Vec2f(rng.unit() * 900.0f - 50.0f, rng.unit() * 700.0f - 50.0f), Vec2f(rng.unit() * 500.0f + 1.0f, rng.unit() * 400.0f + 1.0f) };
}

PanelTable random_panels(Lcg& rng)
{
    PanelTable panels = make_panels();
    for (uint32_t i = 0; i + 1 < k_name_count; ++i)
        if (rng.below(3) == 0)
            set_flags(panels, k_names[i], static_cast<uint8_t>(panel_flag::all & ~(1u << rng.below(6))));
    return panels;
}

DockLayout starting_layout(const PanelTable& panels)
{
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("vp"), k_dock_root, DropZone::Centre));
    std::ignore = dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Right);
    return layout;
}

void random_op(Lcg& rng, DockLayout& layout, const PanelTable& panels)
{
    const PanelId panel = random_panel(rng);
    switch (rng.below(10))
    {
    case 0: std::ignore = dock_panel(layout, panels, panel, k_dock_root, random_zone(rng)); break;
    case 1: std::ignore = dock_panel(layout, panels, panel, random_node(rng, layout, DockNodeKind::Tabs), random_zone(rng)); break;
    case 2: std::ignore = float_panel(layout, panels, panel, random_rect(rng)); break;
    case 3: std::ignore = close_panel(layout, panels, panel); break;
    case 4: std::ignore = open_panel(layout, panels, panel); break;
    case 5: std::ignore = set_collapsed(layout, panels, random_node(rng, layout, DockNodeKind::Tabs), (rng.next() & 1u) != 0); break;
    case 6: std::ignore = set_split(layout, panels, random_node(rng, layout, DockNodeKind::Split), static_cast<DockSizeMode>(rng.below(3)), rng.unit(), rng.unit() * 400.0f); break;
    case 7: std::ignore = select_tab(layout, random_node(rng, layout, DockNodeKind::Tabs), rng.below(k_max_dock_tabs)); break;
    case 8: std::ignore = reorder_tab(layout, panels, panel, rng.below(k_max_dock_tabs)); break;
    default: std::ignore = set_group_open(layout, panels, make_panel_id("none"), true); break;
    }
}

bool inside(const Rect& inner, const Rect& outer)
{
    return inner.min[0] >= outer.min[0] - 0.01f && inner.min[1] >= outer.min[1] - 0.01f && inner.min[0] + inner.size[0] <= outer.min[0] + outer.size[0] + 0.01f && inner.min[1] + inner.size[1] <= outer.min[1] + outer.size[1] + 0.01f;
}

bool round_trips(const DockLayout& layout, const PanelTable& panels)
{
    std::string yaml;
    DockLayout loaded;
    return serialize_layout_to_string(layout, panels, yaml) && deserialize_layout_from_string(loaded, panels, yaml) && diff(layout, loaded).empty();
}

// The invariant set of the docking audit, as the first violation found or an empty string.
std::string first_violation(const DockLayout& layout, const PanelTable& panels, const SolvedLayout& solved, bool roundtrip)
{
    try
    {
        validate(layout, panels, ValidateFlags{ true });
    }
    catch (const oryx::Error& error)
    {
        return std::string("validate: ") + error.what() + "\n" + dump(layout, panels);
    }

    const DockMetrics& metrics = solved.metrics;
    for (uint32_t n = 0; n < layout.node_count; ++n)
    {
        const DockNode& node = layout.nodes[n];
        const SolvedNode& s = solved.nodes[n];
        if (node.kind != DockNodeKind::Tabs || s.rect.size[0] < rail_width(metrics) || s.rect.size[1] < metrics.strip_height)
            continue;
        const std::string where = "node " + std::to_string(n) + "\n" + dump(layout, panels);
        if (s.collapsed && is_empty(s.collapse_button))
            return "collapsed node without an expander: " + where;
        if (s.collapsed && !is_empty(s.body))
            return "collapsed node with a body: " + where;
        if (is_empty(s.strip) || s.visible_count < 1)
            return "unreachable tabs: " + where;
        for (uint32_t t = s.first_visible; t < s.first_visible + s.visible_count; ++t)
            if (is_empty(s.tab_rects[t]))
                return "empty visible tab: " + where;
    }

    for (uint32_t f = 0; f < layout.float_count; ++f)
    {
        if (layout.floats[f].surface != solved.surface)
            continue;
        const Rect& rect = solved.floats[f];
        const std::string where = "float " + std::to_string(f) + "\n" + dump(layout, panels);
        if (!inside(rect, solved.surface_rect))
            return "float off the surface: " + where;
        if (is_empty(solved.float_parts[f].title))
            return "float without a title bar: " + where;
        const Vec2f floor = float_min_size(panels, layout.floats[f].panel, metrics);
        if ((floor[0] <= solved.surface_rect.size[0] && rect.size[0] < floor[0] - 0.01f) || (floor[1] <= solved.surface_rect.size[1] && rect.size[1] < floor[1] - 0.01f))
            return "float below its minimum: " + where;
    }

    if (roundtrip && !round_trips(layout, panels))
        return "save/load does not round-trip\n" + dump(layout, panels);
    return {};
}

} // namespace

TEST_CASE("dock sweep: random op sequences keep every layout invariant")
{
    for (uint32_t seed = 1; seed <= 300; ++seed)
    {
        Lcg rng;
        rng.state = seed * 2654435761u;
        const PanelTable panels = random_panels(rng);
        DockLayout layout = starting_layout(panels);
        LayoutHistory history;
        reset(history, layout);
        for (uint32_t step = 0; step < 60; ++step)
        {
            INFO("seed " << seed << " step " << step);
            random_op(rng, layout, panels);
            if (step % 7 == 6 && can_undo(history))
            {
                DockLayout earlier;
                REQUIRE(undo(history, earlier));
                layout = earlier;
            }
            else
            {
                push(history, layout);
            }
            const float scale = k_scales[rng.below(3)];
            DockMetrics metrics;
            metrics.strip_height *= scale;
            metrics.splitter *= scale;
            metrics.tab_min_width *= scale;
            metrics.tab_max_width *= scale;
            metrics.strip_button *= scale;
            const Rect surface{ Vec2f(0.0f, 0.0f), Vec2f(300.0f + rng.unit() * 800.0f, 200.0f + rng.unit() * 700.0f) };
            const std::string violation = first_violation(layout, panels, solve(layout, panels, metrics, surface), step % 4 == 0);
            REQUIRE_MESSAGE(violation.empty(), violation);
        }
    }
}

namespace
{

struct SweepScene
{
    GuiFixture f;
    DockLayout& layout = f.context.dock_model().layout;

    SweepScene(Lcg& rng)
    {
        f.driver.input().surface_size = { 800.0f, 600.0f };
        model().panels = random_panels(rng);
        layout = starting_layout(model().panels);
        for (uint32_t i = 0; i < 20; ++i)
            random_op(rng, layout, model().panels);
        settle_dock(model());
        frames(4);
    }

    DockView& host() { return f.context.dock_view(); }
    DockModel& model() { return f.context.dock_model(); }

    void draw()
    {
        PanelHostScope scope;
        for (const char* name : k_names)
        {
            PanelScope panel(name);
            if (panel.visible())
                gui::label("body");
        }
    }

    void frames(uint32_t count = 2)
    {
        auto build = f.frame_of([this] { draw(); });
        f.driver.run_frames(count, build);
    }

    Vec2f random_point(Lcg& rng)
    {
        const SolvedLayout& solved = host().solved;
        const Vec2f size = f.driver.input().surface_size;
        const uint32_t pick = rng.below(6);
        if (pick == 0)
            return Vec2f(rng.unit() * (size[0] + 200.0f) - 100.0f, rng.unit() * (size[1] + 200.0f) - 100.0f);
        const int32_t node = random_node(rng, layout, pick < 4 ? DockNodeKind::Tabs : DockNodeKind::Split);
        if (node >= 0 && node < static_cast<int32_t>(solved.node_count))
        {
            const SolvedNode& s = solved.nodes[node];
            if (pick == 1 && !is_empty(s.collapse_button))
                return rect_centre(s.collapse_button);
            if (pick <= 2 && s.visible_count > 0)
                return rect_centre(s.tab_rects[s.first_visible + rng.below(s.visible_count)]);
            if (pick == 3)
                return rect_centre(s.rect);
            if (pick >= 4 && !is_empty(s.splitter))
                return rect_centre(s.splitter);
        }
        if (layout.float_count > 0)
        {
            const uint32_t f_index = rng.below(layout.float_count);
            const Rect& rect = solved.floats[f_index];
            return pick == 5 ? Vec2f(rect.min[0] + rect.size[0] - 3.0f, rect.min[1] + rect.size[1] - 3.0f) : rect_centre(solved.float_parts[f_index].title);
        }
        return Vec2f(rng.unit() * size[0], rng.unit() * size[1]);
    }

    void drag(Lcg& rng, bool escape)
    {
        f.driver.move_to(random_point(rng));
        frames(1);
        f.driver.press();
        frames(1);
        const Vec2f from = f.driver.input().pointer.position;
        const Vec2f to = random_point(rng);
        for (uint32_t step = 1; step <= 5; ++step)
        {
            f.driver.move_to(from + (to - from) * (static_cast<float>(step) / 5.0f));
            frames(1);
        }
        if (escape)
            f.driver.key_press(ImKey::Escape);
        frames(1);
        f.driver.release();
        frames(2);
    }

    void click(Lcg& rng)
    {
        auto build = f.frame_of([this] { draw(); });
        f.driver.click(random_point(rng), build);
        frames(1);
    }

    void change_scale(Lcg& rng)
    {
        GuiTheme theme = f.theme;
        theme.tab.text_height = 16.0f * k_scales[rng.below(3)];
        f.context.set_theme(theme);
    }
};

} // namespace

TEST_CASE("dock sweep: synthetic input through the panel host keeps every invariant")
{
    for (uint32_t seed = 1; seed <= 12; ++seed)
    {
        Lcg rng;
        rng.state = seed * 40503u + 7u;
        SweepScene scene(rng);
        for (uint32_t step = 0; step < 30; ++step)
        {
            INFO("seed " << seed << " step " << step);
            switch (rng.below(8))
            {
            case 0:
            case 1: scene.click(rng); break;
            case 2:
            case 3: scene.drag(rng, false); break;
            case 4: scene.drag(rng, true); break;
            case 5: undo_layout(); break;
            case 6: redo_layout(); break;
            default:
                if (rng.below(2) == 0)
                    scene.f.driver.input().surface_size = { 400.0f + rng.unit() * 600.0f, 300.0f + rng.unit() * 500.0f };
                else
                    scene.change_scale(rng);
                break;
            }
            scene.frames(3);
            const std::string violation = first_violation(scene.layout, scene.model().panels, scene.host().solved, step % 3 == 0);
            REQUIRE_MESSAGE(violation.empty(), violation);
            const PanelId focus = scene.model().focused;
            REQUIRE_MESSAGE((!is_valid(focus) || is_open(scene.layout, focus)), "focus points at a closed panel");
        }
    }
}
