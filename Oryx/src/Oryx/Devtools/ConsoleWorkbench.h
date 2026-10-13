#pragma once

#include "Oryx/Core/LogCapture.h"
#include "Oryx/Debug/ConsoleCommands.h"
#include "Oryx/Interface/Workbench/Workbenches.h"

namespace oryx
{

inline constexpr uint32_t k_console_input_capacity = 256;
inline constexpr uint32_t k_console_search_capacity = 64;
inline constexpr uint32_t k_console_levels = 6;

// What the console keeps between runs.
struct ConsoleState
{
    // The lowest level shown, as an index into trace, debug, info, warning, error, critical.
    int32_t level = 0;
    bool follow = true;
    std::string search;
};

template<typename Visitor>
void reflect(Visitor& v, ConsoleState& state)
{
    v.field("level", state.level);
    v.field("follow", state.follow);
    v.field("search", state.search);
}

// The captured log with a command line: level filter, search, follow-the-tail that pauses when the wheel moves away from it, row selection by click, shift-click or drag with Cmd/Ctrl+C, Tab completion and history over the console command table.
// It is a panel of its own and, through draw_console, the body of the app's bottom drawer; the status bar item shows the warning and error counts of the held lines and flips the drawer's flag.
class ConsoleWorkbench : public Workbench
{
public:
    // `drawer_open` is the flag of the app's console drawer.
    explicit ConsoleWorkbench(bool& drawer_open);
    ~ConsoleWorkbench() noexcept override;

    void status() override;
    void command(CommandHandle command, const CommandArgs& args) override;

    // The controls and the list in the current container.
    void draw_console();
    // Runs one line as the input field would; for tests and for code that wants a command run.
    void run_line(std::string_view line);

    // The lowest level shown (0 trace ... 5 critical) and the text a line must contain (any case); what the controls set.
    void set_level(int32_t level);
    void set_search(std::string_view text);

    [[nodiscard]] uint32_t visible_count() const { return m_visible_count; }
    [[nodiscard]] uint32_t warnings() const { return m_warnings; }
    [[nodiscard]] uint32_t errors() const { return m_errors; }
    [[nodiscard]] bool following() const { return m_state.follow; }

protected:
    void declare_panels() override;
    void draw_panel(PanelHandle panel, const PanelFrame& frame) override;
    void tick(const WorkbenchFrame& frame) override;
    void state_loaded() override;

private:
    void refresh();
    void rebuild_visible();
    void clear_log();
    void copy_selection(bool everything);
    void complete_input();
    [[nodiscard]] bool selected(uint64_t sequence) const;

    bool& m_drawer_open;
    CommandHandle m_clear;
    CommandHandle m_toggle;
    ConsoleState m_state;
    ConsoleHistory m_history;
    std::vector<LogLine> m_lines;
    std::vector<uint16_t> m_visible;
    uint32_t m_count = 0;
    uint32_t m_visible_count = 0;
    uint64_t m_snapshot_sequence = UINT64_MAX;
    uint32_t m_warnings = 0;
    uint32_t m_errors = 0;
    bool m_dirty = true;
    // The selected rows are the lines whose sequence lies between these (either order); zero is none.
    uint64_t m_select_from = 0;
    uint64_t m_select_to = 0;
    bool m_dragging = false;
    char m_input[k_console_input_capacity] = {};
    char m_search[k_console_search_capacity] = {};
};

} // namespace oryx
