#include "oxpch.h"

#include "Oryx/Dashboard/View/DashboardViewRegistry.h"
#include "Oryx/Dashboard/View/ViewFormat.h"
#include "Oryx/Interface/GUI/Gui.h"
#include "Oryx/Interface/GUI/GuiWidgets.h"

namespace oryx
{

namespace
{

constexpr float k_row_height = 22.0f;
constexpr float k_trace_height = 220.0f;
constexpr float k_pin_slack = 1.0f;

struct TraceState
{
    float pinned_offset = 0.0f;
    bool following = false;
};

class TraceView final : public IDashboardView
{
public:
    [[nodiscard]] std::string_view title() const override { return "Trace"; }
    [[nodiscard]] DashboardViewPlacement placement() const override { return { gui::DropZone::Bottom, k_trace_height, false }; }

    void draw(DashboardModel& model) override
    {
        const DashboardFeed& feed = *model.feed;
        if (feed.size() == 0)
        {
            gui::label("Waiting for the first decision");
            return;
        }
        handle_keys(model);
        draw_follow_toggle(model);
        draw_table(model);
    }

private:
    static uint64_t oldest_sequence(const DashboardFeed& feed) { return feed.view(0).record->sequence; }
    static uint64_t latest_sequence(const DashboardFeed& feed) { return feed.view(feed.size() - 1).record->sequence; }

    void handle_keys(DashboardModel& model)
    {
        if (!model.keyboard_focus)
        {
            return;
        }
        const ImKeys& keys = gui::context().input().keys;
        const DashboardFeed& feed = *model.feed;
        const uint64_t oldest = oldest_sequence(feed);
        const uint64_t latest = latest_sequence(feed);
        const RecordView current = selected_view(model);
        const uint64_t at = current.valid ? current.record->sequence : latest;
        if (key_stroke(keys, ImKey::Up))
        {
            select_record(model, at > oldest ? at - 1 : oldest);
            m_reveal = true;
        }
        else if (key_stroke(keys, ImKey::Down))
        {
            select_record(model, at < latest ? at + 1 : latest);
            m_reveal = true;
        }
        else if (key_pressed(keys, ImKey::Home))
        {
            select_record(model, oldest);
            m_reveal = true;
        }
        else if (key_pressed(keys, ImKey::End))
        {
            follow_latest_record(model);
        }
    }

    static void draw_follow_toggle(DashboardModel& model)
    {
        bool follow = model.follow_latest;
        if (gui::toggle("Follow latest", follow))
        {
            if (follow)
            {
                follow_latest_record(model);
            }
            else
            {
                select_record(model, latest_sequence(*model.feed));
            }
        }
    }

    void draw_table(DashboardModel& model)
    {
        const DashboardFeed& feed = *model.feed;
        FrameArena& arena = gui::context().arena();
        TraceState& pin = gui::state<TraceState>(gui::id("trace-state"));
        const RecordView selected = selected_view(model);
        const uint64_t oldest = oldest_sequence(feed);

        gui::TableOptions options;
        options.row_count = static_cast<uint32_t>(feed.size());
        options.row_height = k_row_height;
        gui::TableScope table("trace", options);
        table.column("#", fixed(56.0f));
        table.column("Player", fixed(52.0f));
        table.column("Action", grow());
        table.column("Value", fixed(64.0f), false, TextAlign::Right);
        table.column("Prob", fixed(56.0f), false, TextAlign::Right);
        place_scroll(model, pin, selected, oldest);
        std::ignore = table.headers();

        const GuiTheme& theme = gui::theme();
        for (uint32_t row = table.first_row(); row < table.last_row(); ++row)
        {
            const RecordView view = feed.view(row);
            const DashboardRecord& record = *view.record;
            const bool is_selected = selected.valid && record.sequence == selected.record->sequence;
            const ItemState state = table.row(row, is_selected);
            if (state.clicked)
            {
                select_record(model, record.sequence);
            }
            table.cell(arena.format("%u.%u", record.match_index + 1, record.decision_index + 1));
            table.begin_cell();
            gui::text_coloured(arena.format("%u", static_cast<uint32_t>(record.player)), theme.palette[record.player % k_palette_size]);
            table.end_cell();
            table.cell(label_view(record.chosen_label));
            table.cell(format_view_number(arena, record.chosen_value));
            table.cell(format_view_number(arena, record.chosen_probability));
        }
    }

    // Following pins the list to its end; a wheel move away from the pin pauses following, like a log tail. A key selection scrolls its row into view instead.
    void place_scroll(DashboardModel& model, TraceState& pin, const RecordView& selected, uint64_t oldest)
    {
        const float end = static_cast<float>(model.feed->size()) * k_row_height;
        const bool was_following = pin.following;
        pin.following = model.follow_latest;
        if (model.follow_latest)
        {
            if (was_following && scroll_offset_now() < pin.pinned_offset - k_pin_slack)
            {
                select_record(model, latest_sequence(*model.feed));
                pin.following = false;
            }
            else
            {
                gui::set_scroll_offset("body", end);
                pin.pinned_offset = gui::scroll_offset("body");
                return;
            }
        }
        if (m_reveal && selected.valid)
        {
            m_reveal = false;
            reveal_row(static_cast<float>(selected.record->sequence - oldest));
        }
    }

    static float scroll_offset_now() { return gui::scroll_offset("body"); }

    static void reveal_row(float row)
    {
        Rect view;
        if (!gui::context().layout_rect(gui::context().id("body"), view))
        {
            return;
        }
        const float top = row * k_row_height;
        const float bottom = top + k_row_height;
        const float offset = gui::scroll_offset("body");
        if (top < offset)
        {
            gui::set_scroll_offset("body", top);
        }
        else if (bottom > offset + view.size[1])
        {
            gui::set_scroll_offset("body", bottom - view.size[1]);
        }
    }

    bool m_reveal = false;
};

OX_REGISTER_DASHBOARD_VIEW(TraceView, "trace")

} // namespace

} // namespace oryx
