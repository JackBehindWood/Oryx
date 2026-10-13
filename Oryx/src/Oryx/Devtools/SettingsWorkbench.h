#pragma once

#include "Oryx/Core/Settings.h"
#include "Oryx/Interface/Workbench/Workbenches.h"

namespace oryx
{

inline constexpr uint32_t k_settings_edit_capacity = 192;
inline constexpr uint32_t k_settings_search_capacity = 64;

// What the settings panel keeps between runs.
struct SettingsPanelState
{
    std::string search;
};

template<typename Visitor>
void reflect(Visitor& v, SettingsPanelState& state)
{
    v.field("search", state.search);
}

// Every reflected settings section as editable rows (a checkbox, a combo or a text field by the field's kind), like an editor's settings page.
// A valid edit is applied at once (commit_settings, so subscribers see it) and a rejected one keeps the typed text with its message and a red outline; a row shows `*` while it differs from the files and a Reset button while it
// differs from the default. Save writes the changed keys to the override file beside the settings file, Revert reloads the files; a reload from elsewhere refreshes the rows by the section's version.
class SettingsWorkbench : public Workbench
{
public:
    SettingsWorkbench();
    ~SettingsWorkbench() noexcept override;

    void command(CommandHandle command, const CommandArgs& args) override;

    // The search box, the Save and Revert buttons and the rows in the current container.
    void draw_settings();
    // What the field's own control does when its value is taken: applies it when valid, otherwise keeps the message. `path` is "section.key"; returns whether it was applied.
    bool edit(std::string_view path, std::string_view text);
    // Save and Revert report through a toast; a SettingsError is shown and logged, not thrown.
    void save();
    void revert();
    void set_search(std::string_view text);

    [[nodiscard]] uint32_t modified_count() const;
    [[nodiscard]] uint32_t invalid_count() const;
    // The message shown under a field; empty when it is valid.
    [[nodiscard]] std::string_view error_of(std::string_view path) const;
    [[nodiscard]] std::string_view text_of(std::string_view path) const;

protected:
    void declare_panels() override;
    void draw_panel(PanelHandle panel, const PanelFrame& frame) override;
    void state_loaded() override;

private:
    struct Row
    {
        std::string key;
        std::string doc;
        SettingsKind kind = SettingsKind::Text;
        std::vector<std::string> choices;
        std::string text;
        std::string saved_text;
        std::string default_text;
        std::string error;
        bool dirty = false;
        char edit[k_settings_edit_capacity] = {};
    };

    struct Section
    {
        std::string name;
        SharedPtr<void> copy;
        uint64_t seen_version = 0;
        std::vector<Row> rows;
    };

    void sync();
    void rebuild(Section& section);
    bool commit(Section& section, Row& row, std::string_view text);
    void draw_row(Section& section, Row& row);
    [[nodiscard]] Row* find_row(std::string_view path, Section** owner = nullptr);
    [[nodiscard]] const Row* find_row(std::string_view path) const;

    CommandHandle m_save;
    CommandHandle m_revert;
    SettingsPanelState m_state;
    std::vector<Section> m_sections;
    std::string m_target;
    char m_search[k_settings_search_capacity] = {};
};

} // namespace oryx
