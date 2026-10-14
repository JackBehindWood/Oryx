#include "oxpch.h"
#include "Oryx/Devtools/SettingsWorkbench.h"

#include "Oryx/Interface/GUI/GuiTheme.h"

namespace oryx
{

namespace
{

constexpr Colour k_invalid = { 0.90f, 0.30f, 0.30f, 1.0f };
constexpr Colour k_caption = { 0.60f, 0.62f, 0.68f, 1.0f };
constexpr float k_toast_seconds = 3.0f;

SettingsText collect(std::string_view section, void* value)
{
    SettingsText text;
    walk_settings(section, value, text);
    return text;
}

} // namespace

SettingsWorkbench::SettingsWorkbench()
    : Workbench("settings")
{
    persist(m_state);
}

SettingsWorkbench::~SettingsWorkbench() noexcept = default;

void SettingsWorkbench::declare_panels()
{
    gui::PanelOptions options;
    options.min_w = 320.0f;
    options.min_h = 200.0f;
    options.initial_open = false;
    options.dock_side = gui::DropZone::Right;
    static_cast<void>(add_panel("settings", "Settings", options));
    m_save = add_command({ "save", "Save settings", {}, false });
    m_revert = add_command({ "revert", "Revert settings", {}, false });
}

void SettingsWorkbench::command(CommandHandle command, const CommandArgs&)
{
    if (command == m_save)
    {
        save();
    }
    else if (command == m_revert)
    {
        revert();
    }
}

void SettingsWorkbench::state_loaded()
{
    std::snprintf(m_search, sizeof(m_search), "%s", m_state.search.c_str());
}

void SettingsWorkbench::set_search(std::string_view text)
{
    m_state.search = std::string(text);
    std::snprintf(m_search, sizeof(m_search), "%s", m_state.search.c_str());
}

void SettingsWorkbench::draw_panel(PanelHandle, const PanelFrame&)
{
    draw_settings();
}

void SettingsWorkbench::sync()
{
    if (m_sections.empty())
    {
        for (const std::string& name : settings_sections())
        {
            m_sections.push_back(Section{ name, nullptr, 0, {} });
        }
    }
    for (Section& section : m_sections)
    {
        if (section.copy == nullptr || section.seen_version != settings_version(section.name))
        {
            rebuild(section);
        }
    }
}

void SettingsWorkbench::rebuild(Section& section)
{
    m_target = settings_override_file().string();
    section.copy = settings_copy(section.name);
    section.seen_version = settings_version(section.name);
    SharedPtr<void> saved = settings_persisted(section.name);
    SharedPtr<void> defaults = settings_defaults(section.name);
    const SettingsText live = collect(section.name, section.copy.get());
    const SettingsText file = collect(section.name, saved.get());
    const SettingsText standard = collect(section.name, defaults.get());

    std::vector<Row> rows;
    for (size_t i = 0; i < live.entries().size(); ++i)
    {
        const SettingsText::Entry& entry = live.entries()[i];
        Row row;
        row.key = entry.key;
        row.doc = entry.doc;
        row.kind = entry.kind;
        row.choices = entry.choices;
        row.text = entry.text;
        row.saved_text = file.entries()[i].text;
        row.default_text = standard.entries()[i].text;
        std::snprintf(row.edit, sizeof(row.edit), "%s", row.text.c_str());
        for (const Row& old : section.rows)
        {
            if (old.key == row.key && !old.error.empty() && old.text == row.text)
            {
                row.error = old.error;
                std::snprintf(row.edit, sizeof(row.edit), "%s", old.edit);
            }
        }
        rows.push_back(std::move(row));
    }
    section.rows = std::move(rows);
}

bool SettingsWorkbench::commit(Section& section, Row& row, std::string_view text)
{
    SettingsText set(SettingsText::Mode::Set, row.key, text);
    walk_settings(section.name, section.copy.get(), set);
    if (!set.error().empty())
    {
        row.error = set.error();
        return false;
    }
    commit_settings(section.name, section.copy.get());
    section.seen_version = settings_version(section.name);
    SettingsText get(SettingsText::Mode::Get, row.key);
    walk_settings(section.name, section.copy.get(), get);
    row.text = get.result();
    row.error.clear();
    row.dirty = false;
    std::snprintf(row.edit, sizeof(row.edit), "%s", row.text.c_str());
    return true;
}

SettingsWorkbench::Row* SettingsWorkbench::find_row(std::string_view path, Section** owner)
{
    for (Section& section : m_sections)
    {
        for (Row& row : section.rows)
        {
            if (path.size() == section.name.size() + 1 + row.key.size() && path.substr(0, section.name.size()) == section.name && path[section.name.size()] == '.' && path.substr(section.name.size() + 1) == row.key)
            {
                if (owner != nullptr)
                {
                    *owner = &section;
                }
                return &row;
            }
        }
    }
    return nullptr;
}

const SettingsWorkbench::Row* SettingsWorkbench::find_row(std::string_view path) const
{
    return const_cast<SettingsWorkbench*>(this)->find_row(path, nullptr);
}

bool SettingsWorkbench::edit(std::string_view path, std::string_view text)
{
    sync();
    Section* section = nullptr;
    Row* row = find_row(path, &section);
    if (row == nullptr)
    {
        throw SettingsError("no setting '" + std::string(path) + "'");
    }
    return commit(*section, *row, text);
}

void SettingsWorkbench::save()
{
    try
    {
        save_settings();
        for (Section& section : m_sections)
        {
            section.copy = nullptr;
        }
        sync();
        gui::context().add_toast("Settings saved to " + std::filesystem::path(m_target).filename().string(), k_toast_seconds);
    }
    catch (const Error& error)
    {
        error.log();
        gui::context().add_toast(std::string("Save failed: ") + error.what(), k_toast_seconds);
    }
}

void SettingsWorkbench::revert()
{
    try
    {
        reload_settings();
        sync();
        gui::context().add_toast("Settings reverted to the files", k_toast_seconds);
    }
    catch (const Error& error)
    {
        error.log();
        gui::context().add_toast(std::string("Revert failed: ") + error.what(), k_toast_seconds);
    }
}

uint32_t SettingsWorkbench::modified_count() const
{
    uint32_t count = 0;
    for (const Section& section : m_sections)
    {
        for (const Row& row : section.rows)
        {
            count += row.text != row.saved_text ? 1 : 0;
        }
    }
    return count;
}

uint32_t SettingsWorkbench::invalid_count() const
{
    uint32_t count = 0;
    for (const Section& section : m_sections)
    {
        for (const Row& row : section.rows)
        {
            count += row.error.empty() ? 0 : 1;
        }
    }
    return count;
}

std::string_view SettingsWorkbench::error_of(std::string_view path) const
{
    const Row* row = find_row(path);
    return row != nullptr ? std::string_view(row->error) : std::string_view();
}

std::string_view SettingsWorkbench::text_of(std::string_view path) const
{
    const Row* row = find_row(path);
    return row != nullptr ? std::string_view(row->text) : std::string_view();
}

void SettingsWorkbench::draw_row(Section& section, Row& row)
{
    FrameArena& arena = gui::context().arena();
    const bool modified = row.text != row.saved_text;
    const std::string_view label = arena.format("%s%s", row.key.c_str(), modified ? " *" : "");

    ImStyle invalid = gui::context().role_style(gui::FieldOptions{}, &GuiTheme::field);
    invalid.border = k_invalid;
    invalid.border_width = 2.0f;
    gui::FieldOptions options;
    if (!row.error.empty())
    {
        options.style = &invalid;
    }

    gui::ColumnScope item(arena.format("%s.%s", section.name.c_str(), row.key.c_str()), { .width = grow() });
    {
        gui::RowScope line("line", { .width = grow(), .align = Align::Centre, .gap = gui::theme().space_md });
        if (row.kind == SettingsKind::Bool)
        {
            bool value = row.text == "true";
            if (gui::checkbox(label, value, options))
            {
                commit(section, row, value ? "true" : "false");
            }
        }
        else if (row.kind == SettingsKind::Choice)
        {
            if (gui::ComboScope combo(label, row.text, options); combo)
            {
                for (const std::string& choice : row.choices)
                {
                    if (combo.item(choice, choice == row.text))
                    {
                        commit(section, row, choice);
                    }
                }
            }
        }
        else
        {
            const gui::TextInputResult typed = gui::text_input(label, row.edit, sizeof(row.edit), options);
            row.dirty = row.dirty || typed.changed;
            if (typed.cancelled)
            {
                std::snprintf(row.edit, sizeof(row.edit), "%s", row.text.c_str());
                row.error.clear();
                row.dirty = false;
            }
            else if (typed.submitted || (row.dirty && !typed.focused))
            {
                row.dirty = false;
                commit(section, row, row.edit);
            }
        }
        if (row.text != row.default_text && gui::small_button("Reset").clicked)
        {
            commit(section, row, row.default_text);
        }
    }
    if (!row.error.empty())
    {
        gui::text_coloured(row.error, k_invalid);
    }
    else if (!row.doc.empty())
    {
        gui::text_coloured(row.doc, k_caption);
    }
}

void SettingsWorkbench::draw_settings()
{
    sync();
    FrameArena& arena = gui::context().arena();
    {
        gui::RowScope controls("settings controls", { .width = grow(), .align = Align::Centre, .gap = gui::theme().space_md });
        if (gui::search_box("search", m_search, sizeof(m_search), "Search settings").changed)
        {
            m_state.search = m_search;
        }
        if (gui::small_button("Save").clicked)
        {
            save();
        }
        if (gui::small_button("Revert").clicked)
        {
            revert();
        }
    }
    gui::label(arena.format("%u unsaved, %u invalid. Save writes %s", modified_count(), invalid_count(), m_target.c_str()));
    gui::label("Only sections described for reflection are listed.");

    gui::ScrollScope body("settings body");
    for (Section& section : m_sections)
    {
        if (!gui::collapsing_header(section.name, true))
        {
            continue;
        }
        for (Row& row : section.rows)
        {
            if (gui::filter_matches(m_state.search, row.key) || gui::filter_matches(m_state.search, row.doc))
            {
                draw_row(section, row);
            }
        }
    }
}

} // namespace oryx
