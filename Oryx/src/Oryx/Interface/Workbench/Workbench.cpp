#include "oxpch.h"
#include "Oryx/Interface/Workbench/Workbench.h"

#include "Oryx/Interface/Workbench/Workbenches.h"

namespace oryx
{

bool shortcut_pressed(const Shortcut& shortcut, const ImKeys& keys)
{
    return has_shortcut(shortcut) && key_pressed(keys, shortcut.key) && keys.shortcut == shortcut.shortcut && keys.shift == shortcut.shift && keys.alt == shortcut.alt;
}

namespace
{

const char* key_name(ImKey key)
{
    switch (key)
    {
    case ImKey::Escape: return "Esc";
    case ImKey::Enter: return "Enter";
    case ImKey::Tab: return "Tab";
    case ImKey::Backspace: return "Backspace";
    case ImKey::Delete: return "Delete";
    case ImKey::Left: return "Left";
    case ImKey::Right: return "Right";
    case ImKey::Up: return "Up";
    case ImKey::Down: return "Down";
    case ImKey::Home: return "Home";
    case ImKey::End: return "End";
    case ImKey::PageUp: return "PageUp";
    case ImKey::PageDown: return "PageDown";
    case ImKey::Space: return "Space";
    case ImKey::A: return "A";
    case ImKey::C: return "C";
    case ImKey::V: return "V";
    case ImKey::X: return "X";
    case ImKey::Count: break;
    }
    return "";
}

#ifdef OX_PLATFORM_MACOS
constexpr const char* k_shortcut_name = "Cmd+";
#else
constexpr const char* k_shortcut_name = "Ctrl+";
#endif

} // namespace

std::string shortcut_text(const Shortcut& shortcut)
{
    if (!has_shortcut(shortcut))
    {
        return std::string();
    }
    std::string text;
    text += shortcut.shortcut ? k_shortcut_name : "";
    text += shortcut.shift ? "Shift+" : "";
    text += shortcut.alt ? "Alt+" : "";
    text += key_name(shortcut.key);
    return text;
}

Workbench::Workbench(std::string name, const WorkbenchOptions& options)
    : m_name(std::move(name))
    , m_state_key("workbench." + m_name)
    , m_options(options)
{
}

Workbench::~Workbench() = default;

void Workbench::attach(WorkbenchContext& context)
{
    m_context = &context;
    if (m_attached)
    {
        return;
    }
    declare_panels();
    if (m_options.rules.count > 0 && !gui::set_group_rules(gui::group(m_name), m_options.rules))
    {
        throw Error("workbench '" + m_name + "': the group rules were refused");
    }
    m_attached = true;
}

void Workbench::detach()
{
    m_context = nullptr;
}

bool Workbench::open() const
{
    return gui::is_group_open(m_name);
}

void Workbench::set_open(bool open)
{
    static_cast<void>(gui::set_group_open(m_name, open));
}

bool Workbench::focused() const
{
    for (uint32_t i = 0; i < m_panel_count; ++i)
    {
        if (gui::is_panel_focused(m_panels[i].id))
        {
            return true;
        }
    }
    return false;
}

std::string_view Workbench::panel_name(PanelHandle panel) const
{
    if (panel.index >= m_panel_count)
    {
        throw Error("workbench '" + m_name + "': no such panel");
    }
    return m_panels[panel.index].id;
}

PanelHandle Workbench::add_panel(std::string_view id, std::string_view title, gui::PanelOptions options)
{
    if (m_panel_count == k_max_workbench_panels)
    {
        throw Error("workbench '" + m_name + "' has more than " + std::to_string(k_max_workbench_panels) + " panels");
    }
    std::string full = m_name + "/" + std::string(id);
    options.title = title;
    options.group = m_name;
    if (!gui::register_panel(full, options))
    {
        throw Error("panel '" + full + "' could not be registered (name taken or panel table full)");
    }
    m_panels[m_panel_count].id = std::move(full);
    return PanelHandle{ m_panel_count++ };
}

CommandHandle Workbench::add_command(const CommandSpec& spec)
{
    if (m_command_count == k_max_workbench_commands)
    {
        throw Error("workbench '" + m_name + "' has more than " + std::to_string(k_max_workbench_commands) + " commands");
    }
    if (spec.id.empty())
    {
        throw Error("workbench '" + m_name + "': a command needs an id");
    }
    for (uint32_t i = 0; i < m_command_count; ++i)
    {
        if (m_commands[i].id == spec.id)
        {
            throw Error("workbench '" + m_name + "': command '" + std::string(spec.id) + "' is already added");
        }
    }
    CommandInfo& info = m_commands[m_command_count];
    info.id = std::string(spec.id);
    info.label = spec.label.empty() ? std::string(spec.id) : std::string(spec.label);
    info.shortcut = spec.shortcut;
    info.global = spec.global;
    info.enabled = true;
    return CommandHandle{ m_command_count++ };
}

const CommandInfo& Workbench::command_info(CommandHandle command) const
{
    if (command.index >= m_command_count)
    {
        throw Error("workbench '" + m_name + "': no such command");
    }
    return m_commands[command.index];
}

void Workbench::set_command_enabled(CommandHandle command, bool enabled)
{
    if (command.index >= m_command_count)
    {
        throw Error("workbench '" + m_name + "': no such command");
    }
    m_commands[command.index].enabled = enabled;
}

void Workbench::restore_state()
{
    if (m_state_restored || m_binding == nullptr)
    {
        return;
    }
    m_state_restored = true;
    const std::string_view saved = gui::layout_extension(m_state_key);
    if (!saved.empty())
    {
        m_binding->read(saved);
        state_loaded();
    }
    m_state_hash = m_binding->hash();
}

void Workbench::save_state()
{
    if (m_binding == nullptr || !m_state_restored)
    {
        return;
    }
    const uint64_t hash = m_binding->hash();
    if (hash != m_state_hash)
    {
        m_state_hash = hash;
        gui::set_layout_extension(m_state_key, m_binding->write());
    }
}

void Workbench::route_shortcuts()
{
    if (m_command_count == 0 || gui::io().wants_keyboard)
    {
        return;
    }
    const ImKeys& keys = gui::context().input().keys;
    const bool has_focus = focused();
    for (uint32_t i = 0; i < m_command_count; ++i)
    {
        const CommandInfo& info = m_commands[i];
        if (info.enabled && (info.global || has_focus) && shortcut_pressed(info.shortcut, keys))
        {
            command(CommandHandle{ i }, CommandArgs{});
        }
    }
}

void Workbench::update(const WorkbenchFrame& frame)
{
    if (!m_attached)
    {
        return;
    }
    const bool is_open = open();
    if (is_open != m_was_open)
    {
        m_was_open = is_open;
        if (is_open)
        {
            opened();
        }
        else
        {
            closed();
        }
    }
    if (!is_open && !m_options.always_update)
    {
        return;
    }
    restore_state();
    route_shortcuts();
    tick(frame);
    save_state();
}

void Workbench::draw()
{
    for (uint32_t i = 0; i < m_panel_count; ++i)
    {
        const std::string& id = m_panels[i].id;
        gui::PanelScope scope(id);
        if (scope.visible())
        {
            PanelFrame frame;
            frame.focused = gui::is_panel_focused(id);
            frame.keyboard_ok = frame.focused && !gui::io().wants_keyboard;
            draw_panel(PanelHandle{ i }, frame);
        }
    }
}

void Workbench::menu()
{
    for (uint32_t i = 0; i < m_command_count; ++i)
    {
        const CommandInfo& info = m_commands[i];
        const std::string hint = shortcut_text(info.shortcut);
        const std::string_view label = hint.empty() ? std::string_view(info.label) : gui::context().arena().format("%s    %s", info.label.c_str(), hint.c_str());
        gui::DisabledScope disabled(!info.enabled);
        if (gui::menu_item(label).clicked)
        {
            command(CommandHandle{ i }, CommandArgs{});
        }
    }
}

} // namespace oryx
