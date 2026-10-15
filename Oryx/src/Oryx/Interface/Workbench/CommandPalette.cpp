#include "oxpch.h"
#include "Oryx/Interface/Workbench/CommandPalette.h"

#include "Oryx/Interface/GUI/GuiModal.h"
#include "Oryx/Interface/GUI/GuiWidgets.h"

namespace oryx
{

namespace
{

constexpr const char* k_modal = "command palette";
constexpr float k_width = 520.0f;

char lower(char c)
{
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

bool word_start(std::string_view text, size_t at)
{
    return at == 0 || !std::isalnum(static_cast<unsigned char>(text[at - 1]));
}

} // namespace

int32_t fuzzy_score(std::string_view query, std::string_view text)
{
    int32_t score = 0;
    size_t at = 0;
    bool previous = false;
    for (const char wanted : query)
    {
        if (wanted == ' ')
        {
            previous = false;
            continue;
        }
        bool found = false;
        for (; at < text.size(); ++at)
        {
            if (lower(text[at]) != lower(wanted))
            {
                previous = false;
                continue;
            }
            score += 1 + (previous ? 4 : 0) + (word_start(text, at) ? 3 : 0) + (score == 0 ? math::max(0, 8 - static_cast<int32_t>(at)) : 0);
            previous = true;
            found = true;
            ++at;
            break;
        }
        if (!found)
        {
            return -1;
        }
    }
    return score;
}

void CommandPalette::open(std::vector<PaletteEntry> entries)
{
    m_entries = std::move(entries);
    m_query[0] = '\0';
    m_ranked[0] = '\1';
    m_cursor = 0;
    m_open = true;
    m_focus = true;
    m_dirty = true;
    m_pending = true;
}

void CommandPalette::remember(std::string_view name)
{
    m_recent.erase(std::remove(m_recent.begin(), m_recent.end(), name), m_recent.end());
    m_recent.insert(m_recent.begin(), std::string(name));
    if (m_recent.size() > k_palette_recent)
    {
        m_recent.pop_back();
    }
}

void CommandPalette::rank()
{
    m_matches.clear();
    m_scores.clear();
    const std::string_view query(m_query);
    for (uint32_t i = 0; i < m_entries.size(); ++i)
    {
        int32_t score = 0;
        if (query.empty())
        {
            const auto recent = std::find(m_recent.begin(), m_recent.end(), m_entries[i].name);
            score = recent == m_recent.end() ? 0 : static_cast<int32_t>(k_palette_recent) - static_cast<int32_t>(recent - m_recent.begin());
        }
        else
        {
            score = math::max(fuzzy_score(query, m_entries[i].label), fuzzy_score(query, m_entries[i].name));
            if (score < 0)
            {
                continue;
            }
        }
        m_matches.push_back(i);
        m_scores.push_back(score);
    }
    std::vector<uint32_t> order(m_matches.size());
    for (uint32_t i = 0; i < order.size(); ++i)
    {
        order[i] = i;
    }
    std::stable_sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) { return m_scores[a] > m_scores[b]; });
    std::vector<uint32_t> sorted(m_matches.size());
    for (size_t i = 0; i < order.size(); ++i)
    {
        sorted[i] = m_matches[order[i]];
    }
    m_matches = std::move(sorted);
    m_cursor = 0;
}

int32_t CommandPalette::draw()
{
    if (!m_open)
    {
        return -1;
    }
    if (m_pending)
    {
        gui::open_modal(k_modal);
        m_pending = false;
    }
    LayoutStyle box = im::widget_box(gui::theme().overlay, {});
    box.width = fixed(k_width);
    box.direction = Direction::Column;
    box.gap = gui::theme().spacing;
    box.align_x = Align::Start;
    box.align_y = Align::Start;
    gui::WidgetOptions options;
    options.layout = &box;
    gui::ModalScope dialog(k_modal, options);
    if (!dialog)
    {
        m_open = false;
        return -1;
    }
    if (m_focus)
    {
        gui::request_text_focus("palette query");
        m_focus = false;
    }
    const gui::TextInputResult field = gui::search_box("palette query", m_query, k_palette_query_capacity, "Type a command", { .width = grow() });
    if (field.changed || m_dirty || std::strcmp(m_query, m_ranked) != 0)
    {
        rank();
        std::strcpy(m_ranked, m_query);
        m_dirty = false;
    }

    const ImKeys& keys = gui::context().input().keys;
    const uint32_t shown = math::min(static_cast<uint32_t>(m_matches.size()), k_palette_rows);
    if (shown > 0)
    {
        if (key_stroke(keys, ImKey::Down))
        {
            m_cursor = (m_cursor + 1) % shown;
        }
        if (key_stroke(keys, ImKey::Up))
        {
            m_cursor = (m_cursor + shown - 1) % shown;
        }
    }

    int32_t chosen = -1;
    FrameArena& arena = gui::context().arena();
    for (uint32_t row = 0; row < shown; ++row)
    {
        const PaletteEntry& entry = m_entries[m_matches[row]];
        gui::push_id(arena.format("%u", row));
        gui::RowScope line("row", { .width = grow(), .gap = gui::theme().space_md, .align = Align::Centre });
        const ItemState state = gui::selectable(entry.label, row == m_cursor, { .full_width = true });
        if (!entry.hint.empty())
        {
            gui::caption(entry.hint);
        }
        if (state.hovered && state.hovered_seconds == 0.0f)
        {
            m_cursor = row;
        }
        if (state.clicked && entry.enabled)
        {
            chosen = static_cast<int32_t>(m_matches[row]);
        }
        gui::pop_id();
    }
    if (shown == 0)
    {
        gui::caption("No matching command");
    }
    if ((field.submitted || key_pressed(keys, ImKey::Enter)) && shown > 0 && m_entries[m_matches[m_cursor]].enabled)
    {
        chosen = static_cast<int32_t>(m_matches[m_cursor]);
    }
    if (chosen >= 0)
    {
        gui::close_modal(k_modal);
        m_open = false;
    }
    return chosen;
}

} // namespace oryx
