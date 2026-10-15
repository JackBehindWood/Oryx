#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

inline constexpr uint32_t k_palette_query_capacity = 96;
inline constexpr uint32_t k_palette_rows = 10;
inline constexpr uint32_t k_palette_recent = 8;

// How well `query` matches `text` as a case-insensitive subsequence, or -1 when it does not. Consecutive letters, letters at the start of a word and an early first letter score higher; an empty query scores 0.
[[nodiscard]] int32_t fuzzy_score(std::string_view query, std::string_view text);

// One thing the palette can run: `name` is what the console is given (`<workbench>.<command>` or a console command), `hint` the shortcut or help shown beside it.
struct PaletteEntry
{
    std::string name;
    std::string label;
    std::string hint;
    bool enabled = true;
};

// Ctrl+Shift+P over the commands of the app: a search box and the best matches, Up/Down and Enter or a click to run, Escape to close. The owner fills the entries when it opens and runs what draw() returns.
class CommandPalette
{
public:
    void open(std::vector<PaletteEntry> entries);
    void close() { m_open = false; }
    [[nodiscard]] bool is_open() const { return m_open; }

    // Draws the dialog while open (inside a GUI frame); the index into the entries given to open() of the entry chosen this frame, or -1.
    [[nodiscard]] int32_t draw();
    [[nodiscard]] const PaletteEntry& entry(int32_t index) const { return m_entries[static_cast<size_t>(index)]; }

    // Remembers a run so it leads the list next time the query is empty.
    void remember(std::string_view name);

private:
    void rank();

    std::vector<PaletteEntry> m_entries;
    std::vector<uint32_t> m_matches;
    std::vector<int32_t> m_scores;
    std::vector<std::string> m_recent;
    char m_query[k_palette_query_capacity] = {};
    char m_ranked[k_palette_query_capacity] = {};
    uint32_t m_cursor = 0;
    bool m_open = false;
    bool m_focus = false;
    bool m_pending = false;
    bool m_dirty = true;
};

} // namespace oryx
