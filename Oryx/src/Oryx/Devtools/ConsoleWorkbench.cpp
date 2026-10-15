#include "oxpch.h"
#include "Oryx/Devtools/ConsoleWorkbench.h"

namespace oryx
{

namespace
{

const char* const k_level_names[k_console_levels] = { "trace", "debug", "info", "warning", "error", "critical" };
const char k_level_letters[k_console_levels] = { 'T', 'D', 'I', 'W', 'E', 'C' };
constexpr float k_row_height = 18.0f;
constexpr uint32_t k_warn_level = 3;
constexpr uint32_t k_error_level = 4;

std::string_view text_of(const LogLine& line)
{
    return std::string_view(line.text, line.length);
}

const Colour& level_colour(const GuiTheme& theme, uint32_t level)
{
    if (level >= k_error_level)
    {
        return theme.log_error;
    }
    return level == k_warn_level ? theme.log_warning : level < 2 ? theme.log_debug : theme.log_info;
}

Colour level_tint(const GuiTheme& theme, uint32_t level)
{
    if (level < k_warn_level)
    {
        return { 0.0f, 0.0f, 0.0f, 0.0f };
    }
    const Colour& base = level >= k_error_level ? theme.log_error : theme.log_warning;
    return { base.r, base.g, base.b, level >= k_error_level ? 0.16f : 0.10f };
}

void emit(spdlog::level::level_enum level, std::string_view text)
{
    Log::message(level, text);
}

constexpr std::string_view k_trace_panel = "dashboard/trace";

} // namespace

ConsoleWorkbench::ConsoleWorkbench(bool& drawer_open)
    : Workbench("console", WorkbenchOptions{ .always_update = true })
    , m_drawer_open(drawer_open)
    , m_lines(k_log_capacity)
    , m_visible(k_log_capacity)
{
    persist(m_state);
}

ConsoleWorkbench::~ConsoleWorkbench() noexcept = default;

void ConsoleWorkbench::declare_panels()
{
    gui::PanelOptions options;
    options.min_w = 240.0f;
    options.min_h = 120.0f;
    options.initial_open = false;
    options.dock_side = gui::DropZone::Bottom;
    options.dock_tabbed_with = k_trace_panel;
    m_log = add_panel("log", "Console", options);
    m_clear = add_command({ "clear", "Clear log", {}, false });
    m_toggle = add_command({ "toggle", "Toggle console", {}, false });
}

void ConsoleWorkbench::command(CommandHandle command, const CommandArgs&)
{
    if (command == m_clear)
    {
        clear_log();
    }
    else if (command == m_toggle)
    {
        toggle_console();
    }
}

void ConsoleWorkbench::toggle_console()
{
    const std::string_view panel = panel_name(m_log);
    if (!gui::is_panel_open(k_trace_panel) && !gui::is_panel_open(panel))
    {
        m_drawer_open = !m_drawer_open;
        return;
    }
    m_drawer_open = false;
    if (gui::toggle_panel(panel).applied && gui::is_panel_open(panel))
    {
        std::ignore = gui::select_panel(panel);
    }
}

bool ConsoleWorkbench::console_shown() const
{
    return m_drawer_open || gui::is_panel_open(panel_name(m_log));
}

void ConsoleWorkbench::draw_panel(PanelHandle, const PanelFrame&)
{
    draw_console();
}

void ConsoleWorkbench::state_loaded()
{
    m_state.level = math::clamp(m_state.level, 0, static_cast<int32_t>(k_console_levels) - 1);
    std::snprintf(m_search, sizeof(m_search), "%s", m_state.search.c_str());
    m_dirty = true;
}

void ConsoleWorkbench::tick(const WorkbenchFrame&)
{
    if (log_capture::sequence() != m_snapshot_sequence || m_dirty)
    {
        refresh();
    }
}

void ConsoleWorkbench::set_level(int32_t level)
{
    m_state.level = math::clamp(level, 0, static_cast<int32_t>(k_console_levels) - 1);
    rebuild_visible();
}

void ConsoleWorkbench::set_search(std::string_view text)
{
    m_state.search = std::string(text);
    std::snprintf(m_search, sizeof(m_search), "%s", m_state.search.c_str());
    rebuild_visible();
}

void ConsoleWorkbench::refresh()
{
    m_dirty = false;
    m_snapshot_sequence = log_capture::sequence();
    m_count = log_capture::snapshot(m_lines.data(), k_log_capacity);
    m_warnings = 0;
    m_errors = 0;
    for (uint32_t i = 0; i < m_count; ++i)
    {
        m_warnings += m_lines[i].level == k_warn_level ? 1 : 0;
        m_errors += m_lines[i].level >= k_error_level ? 1 : 0;
    }
    rebuild_visible();
}

void ConsoleWorkbench::rebuild_visible()
{
    m_visible_count = 0;
    for (uint32_t i = 0; i < m_count; ++i)
    {
        const LogLine& line = m_lines[i];
        if (static_cast<int32_t>(line.level) >= m_state.level && gui::filter_matches(m_state.search, text_of(line)))
        {
            m_visible[m_visible_count++] = static_cast<uint16_t>(i);
        }
    }
}

void ConsoleWorkbench::clear_log()
{
    log_capture::clear();
    m_select_from = 0;
    m_select_to = 0;
    m_dirty = true;
}

bool ConsoleWorkbench::selected(uint64_t sequence) const
{
    if (m_select_from == 0)
    {
        return false;
    }
    const uint64_t low = math::min(m_select_from, m_select_to);
    const uint64_t high = math::max(m_select_from, m_select_to);
    return sequence >= low && sequence <= high;
}

void ConsoleWorkbench::copy_selection(bool everything)
{
    std::string text;
    for (uint32_t row = 0; row < m_visible_count; ++row)
    {
        const LogLine& line = m_lines[m_visible[row]];
        if (everything || selected(line.sequence))
        {
            text.append(text_of(line));
            text += '\n';
        }
    }
    if (!text.empty())
    {
        gui::context().request_copy(text);
    }
}

void ConsoleWorkbench::copy_line(uint64_t sequence)
{
    for (uint32_t index = 0; index < m_count; ++index)
    {
        if (m_lines[index].sequence == sequence)
        {
            gui::context().request_copy(std::string(text_of(m_lines[index])));
            return;
        }
    }
}

void ConsoleWorkbench::complete_input()
{
    std::string line(m_input);
    std::vector<std::string> options;
    if (console::complete(line, options, &context().workbenches.commands()))
    {
        std::snprintf(m_input, sizeof(m_input), "%s", line.c_str());
    }
    if (options.size() > 1)
    {
        std::string listing;
        for (const std::string& option : options)
        {
            listing += option;
            listing += "  ";
        }
        emit(spdlog::level::info, listing);
    }
}

void ConsoleWorkbench::run_line(std::string_view line)
{
    emit(spdlog::level::info, "> " + std::string(line));
    m_history.push(line);
    const ConsoleResult result = console::run(line, &context().workbenches.commands());
    std::string_view rest = result.output;
    while (!rest.empty())
    {
        const size_t end = rest.find('\n');
        const std::string_view piece = rest.substr(0, end);
        if (!piece.empty())
        {
            emit(result.ok ? spdlog::level::info : spdlog::level::err, piece);
        }
        rest = end == std::string_view::npos ? std::string_view() : rest.substr(end + 1);
    }
    if (result.clear)
    {
        clear_log();
    }
}

void ConsoleWorkbench::status()
{
    FrameArena& arena = gui::context().arena();
    const GuiTheme& theme = gui::theme();
    if (gui::selectable("Console", console_shown(), { .full_width = false }).clicked)
    {
        toggle_console();
    }
    if (m_errors > 0)
    {
        gui::badge(arena.format("%u errors", m_errors), theme.log_error);
    }
    if (m_warnings > 0)
    {
        gui::badge(arena.format("%u warnings", m_warnings), theme.log_warning);
    }
}

void ConsoleWorkbench::draw_console()
{
    FrameArena& arena = gui::context().arena();
    const ImInput& input = gui::context().input();
    bool filter_changed = false;
    {
        gui::RowScope controls("console controls", { .width = grow(), .gap = gui::theme().space_md, .align = Align::Centre });
        {
            gui::ComboScope level("level", k_level_names[m_state.level], { .label_width = 0.0f, .width = fixed(100.0f) });
            for (int32_t index = 0; index < static_cast<int32_t>(k_console_levels); ++index)
            {
                if (level.item(k_level_names[index], index == m_state.level))
                {
                    m_state.level = index;
                    filter_changed = true;
                }
            }
        }
        if (gui::search_box("search", m_search, sizeof(m_search), "Filter log").changed)
        {
            m_state.search = m_search;
            filter_changed = true;
        }
        if (gui::small_button("Clear").clicked)
        {
            clear_log();
        }
        if (gui::small_button("Copy").clicked)
        {
            copy_selection(m_select_from == 0);
        }
        gui::checkbox("Follow", m_state.follow, { .label_width = 0.0f });
    }
    if (filter_changed)
    {
        rebuild_visible();
    }
    if (!log_capture::available())
    {
        gui::label("log capture not built (forge option log_capture); commands still work");
    }

    bool row_pressed = false;
    bool wheel_over_rows = false;
    if (m_state.follow)
    {
        gui::set_scroll_offset("log", 1.0e9f);
    }
    {
        gui::ListBoxScope list("log", { .height = grow(), .item_count = m_visible_count, .item_height = k_row_height });
        for (uint32_t index = list.first_item(); index < list.last_item() && index < m_visible_count; ++index)
        {
            const LogLine& line = m_lines[m_visible[index]];
            const std::string_view label = arena.format("%c %s  %.*s", k_level_letters[math::min<uint32_t>(line.level, k_console_levels - 1)], line.source == LogSource::Core ? "ORYX" : "APP ", static_cast<int32_t>(line.length), line.text);
            const ItemState row = list.item(index, label, selected(line.sequence), level_colour(gui::theme(), line.level), level_tint(gui::theme(), line.level));
            wheel_over_rows = wheel_over_rows || (row.hovered && input.wheel[1] != 0.0f);
            if (row.right_clicked)
            {
                m_menu_sequence = line.sequence;
                m_menu_level = line.level;
                if (!selected(line.sequence))
                {
                    m_select_from = line.sequence;
                    m_select_to = line.sequence;
                }
                gui::open_popup("log menu");
            }
            if (row.pressed)
            {
                row_pressed = true;
                m_dragging = true;
                if (input.keys.shift && m_select_from != 0)
                {
                    m_select_to = line.sequence;
                }
                else
                {
                    m_select_from = line.sequence;
                    m_select_to = line.sequence;
                }
            }
            else if (m_dragging && row.hovered)
            {
                m_select_to = line.sequence;
            }
        }
    }
    if (gui::PopupScope menu("log menu"); menu)
    {
        if (gui::menu_item("Copy line").clicked)
        {
            copy_line(m_menu_sequence);
            gui::close_current_popup();
        }
        if (gui::menu_item("Copy selection").clicked)
        {
            copy_selection(false);
            gui::close_current_popup();
        }
        if (gui::menu_item("Show only this level").clicked)
        {
            m_state.level = static_cast<int32_t>(math::min<uint32_t>(m_menu_level, k_console_levels - 1));
            rebuild_visible();
            gui::close_current_popup();
        }
        if (gui::menu_item("Clear").clicked)
        {
            clear_log();
            gui::close_current_popup();
        }
    }
    if (!button_of(input, MouseCode::Left).down)
    {
        m_dragging = false;
    }
    if (button_of(input, MouseCode::Left).pressed && !row_pressed && !wheel_over_rows)
    {
        m_select_from = 0;
        m_select_to = 0;
    }
    if (wheel_over_rows)
    {
        m_state.follow = false;
    }
    if (m_select_from != 0 && input.keys.shortcut && key_pressed(input.keys, ImKey::C) && !gui::io().wants_keyboard)
    {
        copy_selection(false);
    }
    if (m_select_from != 0 && input.keys.shortcut && key_pressed(input.keys, ImKey::A) && !gui::io().wants_keyboard)
    {
        if (m_visible_count > 0)
        {
            m_select_from = m_lines[m_visible[0]].sequence;
            m_select_to = m_lines[m_visible[m_visible_count - 1]].sequence;
        }
    }

    const gui::TextInputResult typed = gui::text_input_hint("##console input", m_input, sizeof(m_input), "Type a command, Tab completes, help lists them");
    if (typed.focused || typed.submitted)
    {
        if (key_pressed(input.keys, ImKey::Tab))
        {
            complete_input();
            gui::request_text_focus("##console input");
        }
        if (key_pressed(input.keys, ImKey::Up))
        {
            std::snprintf(m_input, sizeof(m_input), "%s", std::string(m_history.previous()).c_str());
        }
        if (key_pressed(input.keys, ImKey::Down))
        {
            std::snprintf(m_input, sizeof(m_input), "%s", std::string(m_history.next()).c_str());
        }
    }
    if (typed.submitted && m_input[0] != '\0')
    {
        run_line(m_input);
        m_input[0] = '\0';
        gui::request_text_focus("##console input");
        m_state.follow = true;
    }
}

} // namespace oryx
