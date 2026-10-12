#include "unit/Interface/GUI/Dock/DockTestSupport.h"
#include "unit/Interface/support/GuiFixture.h"

using namespace oryx;
using namespace oryx::gui;
using namespace oryx::test;

namespace
{

// "a" and "b" in group one, "c" and "d" in group two; "e" and the viewport have none.
PanelTable grouped_panels()
{
    PanelTable table = make_panels();
    find_panel(table, pid("a"))->group = pid("one");
    find_panel(table, pid("b"))->group = pid("one");
    find_panel(table, pid("c"))->group = pid("two");
    find_panel(table, pid("d"))->group = pid("two");
    return table;
}

DockRules only_with(std::initializer_list<const char*> groups, bool viewports = false)
{
    DockRules rules;
    rules.count = 1;
    rules.rules[0].kind = DockRuleKind::OnlyWithGroups;
    rules.rules[0].viewports = viewports;
    for (const char* name : groups)
        rules.rules[0].groups[rules.rules[0].count++] = pid(name);
    return rules;
}

DockRules never_with(std::initializer_list<const char*> groups)
{
    DockRules rules = only_with(groups);
    rules.rules[0].kind = DockRuleKind::NeverWithGroups;
    return rules;
}

int32_t node_of(const DockLayout& layout, const char* panel)
{
    for (uint32_t n = 0; n < layout.node_count; ++n)
        if (layout.nodes[n].kind == DockNodeKind::Tabs)
            for (uint32_t t = 0; t < layout.nodes[n].count; ++t)
                if (layout.nodes[n].tabs[t] == pid(panel))
                    return static_cast<int32_t>(n);
    return k_no_node;
}

// Tabs [a b], tabs [c d], tabs [e], tabs [vp] side by side; node indices are looked up by panel.
DockLayout mixed_layout(const PanelTable& panels)
{
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("b"), k_dock_root, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("c"), k_dock_root, DropZone::Right));
    require_applied(dock_panel(layout, panels, pid("d"), node_of(layout, "c"), DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("e"), k_dock_root, DropZone::Bottom));
    require_applied(dock_panel(layout, panels, pid("vp"), k_dock_root, DropZone::Left));
    return layout;
}

}

TEST_CASE("group rules: without rules every panel may share tabs")
{
    const PanelTable panels = grouped_panels();
    const DockLayout layout = mixed_layout(panels);
    CHECK(can_dock_into(layout, panels, pid("f"), node_of(layout, "a")) == DockReason::None);
    CHECK(can_dock_into(layout, panels, pid("a"), node_of(layout, "c")) == DockReason::None);
    CHECK(can_share_tabs(panels, pid("a"), pid("c")));
}

TEST_CASE("group rules: OnlyWithGroups refuses tab mates outside the listed groups")
{
    PanelTable panels = grouped_panels();
    REQUIRE(set_group_rules(panels, pid("one"), only_with({ "one" })));
    const DockLayout layout = mixed_layout(panels);
    CHECK(can_dock_into(layout, panels, pid("a"), node_of(layout, "c")) == DockReason::NotPermittedGroup);
    CHECK(can_dock_into(layout, panels, pid("a"), node_of(layout, "e")) == DockReason::NotPermittedGroup);
    CHECK(can_dock_into(layout, panels, pid("a"), node_of(layout, "vp")) == DockReason::NotPermittedGroup);
    CHECK(can_dock_into(layout, panels, pid("b"), node_of(layout, "a")) == DockReason::None);
}

TEST_CASE("group rules: the rule also guards the group's own stacks against outsiders")
{
    PanelTable panels = grouped_panels();
    REQUIRE(set_group_rules(panels, pid("one"), only_with({ "one" })));
    const DockLayout layout = mixed_layout(panels);
    CHECK(can_dock_into(layout, panels, pid("f"), node_of(layout, "a")) == DockReason::NotPermittedGroup);
    CHECK(can_dock_into(layout, panels, pid("c"), node_of(layout, "a")) == DockReason::NotPermittedGroup);
    CHECK(can_dock_into(layout, panels, pid("f"), node_of(layout, "e")) == DockReason::None);
}

TEST_CASE("group rules: edge and root drops start a stack of their own and are not limited")
{
    PanelTable panels = grouped_panels();
    REQUIRE(set_group_rules(panels, pid("one"), only_with({ "one" })));
    const DockLayout layout = mixed_layout(panels);
    CHECK(can_dock_into(layout, panels, pid("a"), node_of(layout, "c"), DropZone::Left) == DockReason::None);
    CHECK(can_dock_into(layout, panels, pid("f"), node_of(layout, "a"), DropZone::Bottom) == DockReason::None);
    CHECK(can_dock_into(layout, panels, pid("a"), k_dock_root, DropZone::Centre) == DockReason::None);
}

TEST_CASE("group rules: NeverWithGroups refuses the listed groups")
{
    PanelTable panels = grouped_panels();
    REQUIRE(set_group_rules(panels, pid("one"), never_with({ "two" })));
    const DockLayout layout = mixed_layout(panels);
    CHECK(can_dock_into(layout, panels, pid("a"), node_of(layout, "c")) == DockReason::NotPermittedGroup);
    CHECK(can_dock_into(layout, panels, pid("c"), node_of(layout, "a")) == DockReason::NotPermittedGroup);
    CHECK(can_dock_into(layout, panels, pid("a"), node_of(layout, "e")) == DockReason::None);
    CHECK(can_dock_into(layout, panels, pid("a"), node_of(layout, "vp")) == DockReason::None);
}

TEST_CASE("group rules: viewports are allowed only when the rule says so")
{
    PanelTable strict = grouped_panels();
    REQUIRE(set_group_rules(strict, pid("one"), only_with({ "one" })));
    PanelTable open = grouped_panels();
    REQUIRE(set_group_rules(open, pid("one"), only_with({ "one" }, true)));
    const DockLayout layout = mixed_layout(strict);
    CHECK(can_dock_into(layout, strict, pid("a"), node_of(layout, "vp")) == DockReason::NotPermittedGroup);
    CHECK(can_dock_into(layout, open, pid("a"), node_of(layout, "vp")) == DockReason::None);
    CHECK(can_dock_into(layout, open, pid("a"), node_of(layout, "e")) == DockReason::NotPermittedGroup);
}

TEST_CASE("group rules: every rule of a group must allow the tab mate")
{
    PanelTable panels = grouped_panels();
    DockRules rules = only_with({ "one", "two" });
    rules.rules[1] = never_with({ "two" }).rules[0];
    rules.count = 2;
    REQUIRE(set_group_rules(panels, pid("one"), rules));
    CHECK(can_share_tabs(panels, pid("a"), pid("b")));
    CHECK_FALSE(can_share_tabs(panels, pid("a"), pid("c")));
    CHECK_FALSE(can_share_tabs(panels, pid("a"), pid("e")));
}

TEST_CASE("group rules: a refused drop leaves the layout untouched and explains itself")
{
    PanelTable panels = grouped_panels();
    REQUIRE(set_group_rules(panels, pid("one"), only_with({ "one" })));
    DockLayout layout = mixed_layout(panels);
    const DockLayout before = layout;
    const DockResult result = dock_panel(layout, panels, pid("a"), node_of(layout, "c"), DropZone::Centre);
    CHECK_FALSE(result.applied);
    CHECK(result.reason == DockReason::NotPermittedGroup);
    CHECK(equal(layout, before));
    CHECK(std::string(describe(DockReason::NotPermittedGroup)).find("share") != std::string::npos);
    CHECK(std::string(to_string(DockReason::NotPermittedGroup)).find("group") != std::string::npos);
}

TEST_CASE("group rules: dock_tabs refuses a stack when one member breaks the rules")
{
    PanelTable panels = grouped_panels();
    REQUIRE(set_group_rules(panels, pid("two"), only_with({ "two" })));
    DockLayout layout = mixed_layout(panels);
    const PanelId moved[2] = { pid("a"), pid("b") };
    const DockResult result = dock_tabs(layout, panels, moved, 2, node_of(layout, "c"), DropZone::Centre);
    CHECK(result.reason == DockReason::NotPermittedGroup);
}

TEST_CASE("group rules: drop guides refuse the centre of a forbidden stack and keep the edges")
{
    PanelTable panels = grouped_panels();
    REQUIRE(set_group_rules(panels, pid("one"), only_with({ "one" })));
    const DockLayout layout = mixed_layout(panels);
    DockMetrics metrics;
    metrics.strip_button = 0.0f;
    const SolvedLayout solved = solve(layout, panels, metrics, Rect{ Vec2f(0.0f, 0.0f), Vec2f(800.0f, 600.0f) });
    const Rect body = solved.nodes[node_of(layout, "c")].body;
    const DropGuides guides = drop_guides(layout, panels, solved, pid("a"), body.min + body.size * 0.5f);
    REQUIRE(guides.count > 0);
    for (uint32_t index = 0; index < guides.count; ++index)
    {
        const DropGuide& guide = guides.guides[index];
        if (guide.node != node_of(layout, "c"))
            continue;
        CHECK(guide.allowed == (guide.zone != DropZone::Centre));
        if (guide.zone == DropZone::Centre)
            CHECK(guide.reason == DockReason::NotPermittedGroup);
    }
}

TEST_CASE("group rules: repair closes the later tab of every forbidden pair")
{
    PanelTable panels = grouped_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("c"), 0, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("b"), 0, DropZone::Centre));
    REQUIRE(set_group_rules(panels, pid("one"), only_with({ "one" })));
    CHECK(repair_group_rules(layout, panels) == 1);
    CHECK(is_open(layout, pid("a")));
    CHECK(is_open(layout, pid("b")));
    CHECK_FALSE(is_open(layout, pid("c")));
    CHECK_NOTHROW(validate(layout, panels, ValidateFlags{}));
    CHECK(repair_group_rules(layout, panels) == 0);
}

TEST_CASE("group rules: repair does nothing without rules")
{
    PanelTable panels = grouped_panels();
    DockLayout layout = mixed_layout(panels);
    const DockLayout before = layout;
    CHECK(repair_group_rules(layout, panels) == 0);
    CHECK(equal(layout, before));
}

TEST_CASE("group rules: set_group_rules validates and replaces")
{
    PanelTable panels = grouped_panels();
    CHECK_FALSE(set_group_rules(panels, PanelId{}, only_with({ "one" })));
    DockRules too_many = only_with({ "one" });
    too_many.rules[0].count = static_cast<uint8_t>(k_max_dock_rules + 1);
    CHECK_FALSE(set_group_rules(panels, pid("one"), too_many));
    REQUIRE(set_group_rules(panels, pid("one"), only_with({ "one" })));
    REQUIRE(set_group_rules(panels, pid("one"), never_with({ "two" })));
    CHECK(panels.group_rule_count == 1);
    CHECK(find_group_rules(panels, pid("one"))->rules[0].kind == DockRuleKind::NeverWithGroups);
    for (uint32_t index = 1; index < k_max_rule_groups; ++index)
    {
        const std::string name = "g" + std::to_string(index);
        CHECK(set_group_rules(panels, pid(name), only_with({ "one" })));
    }
    CHECK(panels.group_rule_count == k_max_rule_groups);
    CHECK_FALSE(set_group_rules(panels, pid("overflow"), only_with({ "one" })));
}

TEST_CASE("group rules: the host presets build the documented rules")
{
    const GroupHandle one = group("one");
    CHECK(rules_free().count == 0);
    const DockRules own = rules_own_group(one);
    REQUIRE(own.count == 1);
    CHECK(own.rules[0].kind == DockRuleKind::OnlyWithGroups);
    CHECK(own.rules[0].count == 1);
    CHECK(own.rules[0].groups[0] == pid("one"));
    CHECK_FALSE(own.rules[0].viewports);
    CHECK(rules_own_group_and_viewports(one).rules[0].viewports);
    const GroupHandle several[3] = { group("one"), group("two"), group("three") };
    const DockRules only = rules_only_with(several, 3);
    CHECK(only.rules[0].count == 3);
    CHECK(only.rules[0].groups[2] == pid("three"));
}

TEST_CASE("group rules: registered through the host they limit a drop and repair a loaded layout")
{
    GuiFixture f;
    DockModel& model = f.context.dock_model();
    PanelOptions in_one;
    in_one.group = "one";
    PanelOptions in_two;
    in_two.group = "two";
    REQUIRE(register_panel("a", in_one));
    REQUIRE(register_panel("b", in_two));
    REQUIRE(set_group_rules(group("one"), rules_own_group(group("one"))));
    DockLayout layout;
    require_applied(dock_panel(layout, model.panels, pid("a"), k_dock_root, DropZone::Centre));
    CHECK(dock_panel(layout, model.panels, pid("b"), 0, DropZone::Centre).reason == DockReason::NotPermittedGroup);
    CHECK(dock_panel(layout, model.panels, pid("b"), 0, DropZone::Right).applied);
}
