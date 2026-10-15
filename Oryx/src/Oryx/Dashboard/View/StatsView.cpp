#include "oxpch.h"

#include "Oryx/Dashboard/View/DashboardViewRegistry.h"
#include "Oryx/Dashboard/View/ViewFormat.h"
#include "Oryx/Interface/GUI/Gui.h"
#include "Oryx/Interface/GUI/GuiWidgets.h"

namespace oryx
{

namespace
{

constexpr float k_name_min_width = 96.0f;
constexpr float k_number_width = 76.0f;
constexpr size_t k_open_by_default_keys = 24;
constexpr std::string_view k_ungrouped = "other";

class StatsView final : public IDashboardView
{
public:
    [[nodiscard]] std::string_view title() const override { return "Stats"; }

    void draw(DashboardModel& model) override
    {
        const DashboardFeed& feed = *model.feed;
        if (feed.key_count() == 0)
        {
            gui::empty_state("No diagnostics yet", "The strategy has not reported any metrics");
            return;
        }
        FrameArena& arena = gui::context().arena();
        const size_t key_count = feed.key_count();
        std::span<uint32_t> group_of = arena.allocate_array<uint32_t>(key_count);
        std::span<double> ply = arena.allocate_array<double>(key_count);
        std::span<std::string_view> groups = arena.allocate_array<std::string_view>(key_count);
        size_t group_count = 0;
        for (size_t key = 0; key < key_count; ++key)
        {
            const std::string_view space = key_namespace(feed.key_name(static_cast<uint16_t>(key)));
            size_t group = 0;
            while (group < group_count && groups[group] != space)
            {
                ++group;
            }
            if (group == group_count)
            {
                groups[group_count++] = space;
            }
            group_of[key] = static_cast<uint32_t>(group);
            ply[key] = std::numeric_limits<double>::quiet_NaN();
        }
        const RecordView selected = selected_view(model);
        for (uint32_t i = 0; selected.valid && i < selected.record->metric_count; ++i)
        {
            ply[selected.metrics[i].key] = selected.metrics[i].value;
        }

        gui::ScrollScope scroll("stats");
        for (size_t group = 0; group < group_count; ++group)
        {
            draw_group(feed, group_of, ply, static_cast<uint32_t>(group), groups[group]);
        }
        draw_drops(feed.drops());
    }

private:
    static void draw_group(const DashboardFeed& feed, std::span<const uint32_t> group_of, std::span<const double> ply, uint32_t group, std::string_view space)
    {
        FrameArena& arena = gui::context().arena();
        uint32_t rows = 0;
        bool single = true;
        for (size_t key = 0; key < group_of.size(); ++key)
        {
            if (group_of[key] == group)
            {
                ++rows;
                single = single && feed.key_stats(static_cast<uint16_t>(key)).count <= 1;
            }
        }
        gui::push_id(arena.format("%u", group));
        if (gui::collapsing_header(space.empty() ? k_ungrouped : space, feed.key_count() <= k_open_by_default_keys))
        {
            gui::TableOptions options;
            options.row_count = rows;
            options.row_height = gui::theme().row_height;
            options.height = fixed(static_cast<float>(rows) * gui::theme().row_height + gui::theme().bar_height);
            gui::TableScope table("keys", options);
            table.column("Name", grow(1.0f, k_name_min_width));
            table.column(single ? "Value" : "Now", fixed(k_number_width), false, TextAlign::Right, true, single ? "Latest value" : "Value at the selected decision");
            if (!single)
            {
                table.column("Last", fixed(k_number_width), false, TextAlign::Right, true, "Most recent value");
                table.column("Mean", fixed(k_number_width), false, TextAlign::Right, true, "Average over the decisions kept");
                table.column("Max", fixed(k_number_width), false, TextAlign::Right, true, "Largest value seen");
                table.column("Count", fixed(k_number_width), false, TextAlign::Right, true, "How many decisions reported this key");
            }
            std::ignore = table.headers();
            uint32_t row = 0;
            for (size_t key = 0; key < group_of.size(); ++key)
            {
                if (group_of[key] != group)
                {
                    continue;
                }
                const std::string_view name = feed.key_name(static_cast<uint16_t>(key));
                const KeyStats& stats = feed.key_stats(static_cast<uint16_t>(key));
                const ItemState state = table.row(row++);
                gui::tooltip(state, name);
                table.cell(space.empty() ? name : name.substr(space.size() + 1));
                table.cell(format_view_number(arena, single ? stats.last : ply[key]));
                if (single)
                {
                    continue;
                }
                table.cell(format_view_number(arena, stats.last));
                table.cell(stats.keeps_max || stats.count == 0 ? "-" : format_view_number(arena, stats.sum / static_cast<double>(stats.count)));
                table.cell(format_view_number(arena, stats.max));
                table.cell(format_view_number(arena, static_cast<double>(stats.count)));
            }
        }
        gui::pop_id();
    }

    static void draw_drops(const FeedDrops& drops)
    {
        if (drops.scores == 0 && drops.metrics == 0 && drops.tree_nodes == 0)
        {
            return;
        }
        FrameArena& arena = gui::context().arena();
        gui::label(arena.format("Dropped: %llu scores, %llu metrics, %llu tree nodes", static_cast<unsigned long long>(drops.scores), static_cast<unsigned long long>(drops.metrics), static_cast<unsigned long long>(drops.tree_nodes)));
    }
};

OX_REGISTER_DASHBOARD_VIEW(StatsView, "stats")

} // namespace

} // namespace oryx
