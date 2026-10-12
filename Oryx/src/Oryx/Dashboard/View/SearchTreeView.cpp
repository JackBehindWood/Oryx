#include "oxpch.h"

#include "Oryx/Dashboard/Feed/TreeIndex.h"
#include "Oryx/Dashboard/View/DashboardViewRegistry.h"
#include "Oryx/Dashboard/View/ViewFormat.h"
#include "Oryx/Interface/GUI/Gui.h"
#include "Oryx/Interface/GUI/GuiWidgets.h"

namespace oryx
{

namespace
{

constexpr float k_row_height = 22.0f;
constexpr float k_indent = 12.0f;
constexpr uint32_t k_path_nodes = 8;

class SearchTreeView final : public IDashboardView
{
public:
    [[nodiscard]] std::string_view title() const override { return "Search"; }

    void draw(DashboardModel& model) override
    {
        const RecordView view = selected_view(model);
        if (!view.valid)
        {
            gui::label(model.feed->size() == 0 ? "Waiting for the first decision" : "No decision to show");
            return;
        }
        if (view.record->tree_count == 0)
        {
            draw_empty(model);
            return;
        }
        sync(view);
        if (view.record->dropped_tree_nodes > 0)
        {
            gui::label(gui::context().arena().format("Truncated: %u nodes were not kept", view.record->dropped_tree_nodes));
        }
        handle_keys(model);
        draw_toolbar();
        draw_table(model, view);
        draw_details(view);
    }

private:
    static void draw_empty(const DashboardModel& model)
    {
        gui::label("No search tree for this decision");
        gui::label(model.feed->options().max_tree_nodes == 0 ? "Tree storage is off: set dashboard.max_tree_nodes" : "The strategy did not publish one");
    }

    void sync(const RecordView& view)
    {
        const bool fresh = view.record->sequence != m_sequence || view.tree != m_tree || view.record->tree_count != m_index.node_count();
        if (!fresh)
        {
            return;
        }
        m_sequence = view.record->sequence;
        m_tree = view.tree;
        m_index.build(view.tree, view.record->tree_count, m_sort, m_descending);
        m_selected = k_no_tree_node;
        expand_principal();
    }

    void rebuild_rows() { m_index.visible_rows(m_expanded.data(), m_rows); }

    void expand_principal()
    {
        m_expanded.assign(m_index.node_count(), 0);
        for (uint32_t node = 0; node < m_index.node_count(); ++node)
        {
            m_expanded[node] = m_index.is_principal(node) ? 1 : 0;
        }
        rebuild_rows();
    }

    void set_all_expanded(uint8_t value)
    {
        std::fill(m_expanded.begin(), m_expanded.end(), value);
        rebuild_rows();
    }

    void toggle(uint32_t node)
    {
        if (m_index.has_children(node))
        {
            m_expanded[node] ^= 1;
            rebuild_rows();
        }
    }

    [[nodiscard]] size_t row_of(uint32_t node) const
    {
        for (size_t row = 0; row < m_rows.size(); ++row)
        {
            if (m_rows[row] == node)
            {
                return row;
            }
        }
        return m_rows.size();
    }

    void select_row(size_t row)
    {
        m_selected = m_rows[row];
        m_reveal = true;
    }

    void handle_keys(const DashboardModel& model)
    {
        if (!model.keyboard_focus || m_rows.empty())
        {
            return;
        }
        const ImKeys& keys = gui::context().input().keys;
        const size_t at = row_of(m_selected);
        if (key_stroke(keys, ImKey::Down))
        {
            select_row(at + 1 < m_rows.size() ? at + 1 : at < m_rows.size() ? at : 0);
        }
        else if (key_stroke(keys, ImKey::Up))
        {
            select_row(at > 0 && at < m_rows.size() ? at - 1 : 0);
        }
        else if (key_pressed(keys, ImKey::Home))
        {
            select_row(0);
        }
        else if (key_pressed(keys, ImKey::End))
        {
            select_row(m_rows.size() - 1);
        }
        else if (at < m_rows.size() && key_stroke(keys, ImKey::Right) && m_index.has_children(m_selected))
        {
            if (m_expanded[m_selected] == 0)
            {
                toggle(m_selected);
            }
            else
            {
                select_row(at + 1);
            }
        }
        else if (at < m_rows.size() && key_stroke(keys, ImKey::Left))
        {
            if (m_expanded[m_selected] != 0 && m_index.has_children(m_selected))
            {
                toggle(m_selected);
            }
            else if (m_index.depth(m_selected) > 0)
            {
                m_selected = m_index.parent(m_selected);
                m_reveal = true;
            }
        }
    }

    void draw_toolbar()
    {
        FrameArena& arena = gui::context().arena();
        gui::begin_row("search-bar");
        if (gui::button("Expand all").clicked)
        {
            set_all_expanded(1);
        }
        if (gui::button("Collapse").clicked)
        {
            set_all_expanded(0);
        }
        if (gui::button("Best line").clicked)
        {
            expand_principal();
        }
        gui::spacer();
        gui::label(arena.format("%u nodes \xC2\xB7 depth %u", m_index.reachable_count(), m_index.max_depth() + 1));
        gui::end_row();
    }

    static std::string_view node_label(const RecordView& view, const TreeIndex& index, uint32_t node, FrameArena& arena)
    {
        const ActionId action = index.node(node).action;
        if (!is_game_action(action))
        {
            return index.depth(node) == 0 ? "Root" : "Pass";
        }
        for (uint32_t i = 0; i < view.record->score_count; ++i)
        {
            if (view.scores[i].action == action)
            {
                return label_view(view.scores[i].label);
            }
        }
        return arena.format("Action %llu", static_cast<unsigned long long>(action));
    }

    // The root's own moves are the nodes the board can point at.
    [[nodiscard]] bool is_root_move(uint32_t node) const { return is_game_action(m_index.node(node).action) && m_index.depth(node) <= 1; }

    void draw_node_cell(const RecordView& view, uint32_t node)
    {
        FrameArena& arena = gui::context().arena();
        gui::IconOptions indent;
        indent.size = static_cast<float>(m_index.depth(node)) * k_indent;
        if (indent.size > 0.0f)
        {
            gui::icon(Icon::None, indent);
        }
        gui::IconOptions chevron;
        chevron.size = 14.0f;
        gui::icon(!m_index.has_children(node) ? Icon::None : m_expanded[node] != 0 ? Icon::ChevronDown : Icon::ChevronRight, chevron);
        const std::string_view text = node_label(view, m_index, node, arena);
        if (m_index.is_principal(node))
        {
            gui::text_coloured(text, gui::theme().panel.accent);
        }
        else
        {
            gui::label(text);
        }
    }

    void draw_table(DashboardModel& model, const RecordView& view)
    {
        FrameArena& arena = gui::context().arena();
        gui::TableOptions options;
        options.row_count = static_cast<uint32_t>(m_rows.size());
        options.row_height = k_row_height;
        gui::TableScope table("search", options);
        table.column("Node", grow(1.0f, 96.0f), true);
        table.column("Visits", fixed(72.0f), true, TextAlign::Right);
        table.column("Value", fixed(64.0f), true, TextAlign::Right);
        table.column("Share", fixed(56.0f), false, TextAlign::Right);
        reveal(options.row_count);
        const gui::TableResult sort = table.headers();
        if (sort.sort_changed)
        {
            m_sort = sort.sort_column == 0 ? TreeSort::Order : sort.sort_column == 1 ? TreeSort::Visits : TreeSort::Value;
            m_descending = !sort.sort_ascending;
            m_index.build(view.tree, view.record->tree_count, m_sort, m_descending);
            rebuild_rows();
        }

        const ActionId linked = linked_action(model);
        for (uint32_t row = table.first_row(); row < table.last_row() && row < m_rows.size(); ++row)
        {
            const uint32_t node = m_rows[row];
            const bool root_move = is_root_move(node);
            const bool lit = node == m_selected || (root_move && m_index.node(node).action == linked);
            const ItemState state = table.row(row, lit);
            if (state.clicked)
            {
                m_selected = node;
                if (root_move)
                {
                    const ActionId action = m_index.node(node).action;
                    model.pinned_action = model.pinned_action == action ? INVALID_ACTION : action;
                }
            }
            if (state.double_clicked)
            {
                toggle(node);
            }
            if (state.hovered && root_move)
            {
                model.hovered_action = m_index.node(node).action;
            }
            table.begin_cell();
            draw_node_cell(view, node);
            table.end_cell();
            const SearchNode& data = m_index.node(node);
            table.cell(format_view_number(arena, static_cast<double>(data.visits)));
            table.cell(format_view_number(arena, data.value));
            const double share = m_index.share_of_parent(node);
            table.cell(std::isnan(share) ? std::string_view("-") : arena.format("%.0f%%", share * 100.0));
        }
    }

    void reveal(uint32_t rows)
    {
        const size_t at = row_of(m_selected);
        if (!m_reveal || at >= rows)
        {
            return;
        }
        m_reveal = false;
        Rect area;
        if (!gui::context().layout_rect(gui::context().id("body"), area))
        {
            return;
        }
        const float top = static_cast<float>(at) * k_row_height;
        const float bottom = top + k_row_height;
        const float offset = gui::scroll_offset("body");
        if (top < offset)
        {
            gui::set_scroll_offset("body", top);
        }
        else if (bottom > offset + area.size[1])
        {
            gui::set_scroll_offset("body", bottom - area.size[1]);
        }
    }

    void draw_details(const RecordView& view)
    {
        if (m_selected >= m_index.node_count())
        {
            gui::label("Select a node (double-click or Left/Right to fold)");
            return;
        }
        FrameArena& arena = gui::context().arena();
        const SearchNode& data = m_index.node(m_selected);
        const double share = m_index.share_of_parent(m_selected);
        const std::string_view name = node_label(view, m_index, m_selected, arena);
        const std::string_view visits = format_view_number(arena, static_cast<double>(data.visits));
        const std::string_view value = format_view_number(arena, data.value);
        const std::string_view of_parent = std::isnan(share) ? std::string_view() : arena.format(" \xC2\xB7 %.0f%% of parent", share * 100.0);
        const std::string_view path = path_text(view, arena);
        gui::separator();
        gui::label(arena.format("%.*s \xC2\xB7 depth %u \xC2\xB7 %.*s visits \xC2\xB7 value %.*s%.*s", static_cast<int32_t>(name.size()), name.data(), m_index.depth(m_selected) + 1, static_cast<int32_t>(visits.size()), visits.data(),
            static_cast<int32_t>(value.size()), value.data(), static_cast<int32_t>(of_parent.size()), of_parent.data()));
        gui::label(arena.format("Path: %.*s", static_cast<int32_t>(path.size()), path.data()));
        gui::label("Value as reported by the strategy; the best line follows the most visited child");
    }

    std::string_view path_text(const RecordView& view, FrameArena& arena) const
    {
        uint32_t chain[k_path_nodes];
        uint32_t length = 0;
        for (uint32_t node = m_selected; length < k_path_nodes; node = m_index.parent(node))
        {
            chain[length++] = node;
            if (m_index.depth(node) == 0)
            {
                break;
            }
        }
        char text[256];
        size_t used = 0;
        for (uint32_t i = length; i > 0 && used + 1 < sizeof(text); --i)
        {
            const std::string_view part = node_label(view, m_index, chain[i - 1], arena);
            const int32_t written = std::snprintf(text + used, sizeof(text) - used, "%s%.*s", i == length ? "" : " > ", static_cast<int32_t>(part.size()), part.data());
            used += written > 0 ? math::min(static_cast<size_t>(written), sizeof(text) - used - 1) : 0;
        }
        return arena.store(std::string_view(text, used));
    }

    TreeIndex m_index;
    std::vector<uint32_t> m_rows;
    std::vector<uint8_t> m_expanded;
    const SearchNode* m_tree = nullptr;
    uint64_t m_sequence = 0;
    uint32_t m_selected = k_no_tree_node;
    TreeSort m_sort = TreeSort::Visits;
    bool m_descending = true;
    bool m_reveal = false;
};

OX_REGISTER_DASHBOARD_VIEW(SearchTreeView, "search")

} // namespace

} // namespace oryx
