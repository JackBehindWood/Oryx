#pragma once

#include "Oryx/Interface/GUI/GuiWidgets.h"
#include "Oryx/Interface/Workbench/IWorkbench.h"
#include "Oryx/Interface/Workbench/WorkbenchState.h"

namespace oryx
{

struct WorkbenchOptions
{
    // Update while no panel of the workbench is open (a counter that must keep counting); otherwise a closed workbench costs no frame time.
    bool always_update = false;
    // Which panels its group may share a tab stack with; the default is any.
    gui::DockRules rules;
};

// What draw_panel is told about the panel it draws into.
struct PanelFrame
{
    bool focused = false;
    // The panel has focus and no text field wants the keys: safe to read shortcuts.
    bool keyboard_ok = false;
};

// The usual workbench: override declare_panels and draw_panel and the rest follows (panels in one dock group, an open/close lifecycle, commands with shortcuts and menu items, saved state). Everything is optional.
//   class Notes : public Workbench { Notes() : Workbench("notes") {} void declare_panels() override { m_list = add_panel("List"); } void draw_panel(PanelHandle, const PanelFrame&) override { gui::label("hello"); } };
class Workbench : public IWorkbench
{
public:
    explicit Workbench(std::string name, const WorkbenchOptions& options = {});
    ~Workbench() override;

    [[nodiscard]] std::string_view name() const override { return m_name; }
    // Registers the panels once (a second attach is ignored) and applies the group rules.
    void attach(WorkbenchContext& context) override;
    void detach() override;
    void update(const WorkbenchFrame& frame) override;
    void draw() override;
    // Lists the commands as menu items with their shortcut.
    void menu() override;
    void status() override {}
    void toolbar() override {}

    [[nodiscard]] uint32_t command_count() const override { return m_command_count; }
    [[nodiscard]] const CommandInfo& command_info(CommandHandle command) const override;
    void command(CommandHandle, const CommandArgs&) override {}

    // Any panel of the workbench is open.
    [[nodiscard]] bool open() const;
    void set_open(bool open);
    [[nodiscard]] bool focused() const;
    [[nodiscard]] std::string_view panel_name(PanelHandle panel) const;
    [[nodiscard]] WorkbenchContext& context() const { return *m_context; }

    void set_command_enabled(CommandHandle command, bool enabled);

protected:
    // Called by attach: register the panels with add_panel and the commands with add_command.
    virtual void declare_panels() {}
    // Inside begin_panel/end_panel of an open panel.
    virtual void draw_panel(PanelHandle, const PanelFrame&) {}
    // When the workbench's group opens or closes; closed() runs once more even for always_update workbenches.
    virtual void opened() {}
    virtual void closed() {}
    // Once per frame while open (or always with always_update), after the state was restored.
    virtual void tick(const WorkbenchFrame&) {}
    // The state struct was just read from the layout file; apply it.
    virtual void state_loaded() {}

    // Registers a panel `<workbench>/<id>` in the workbench's group; `options.group` is set for you.
    PanelHandle add_panel(std::string_view id, std::string_view title, gui::PanelOptions options = {});
    CommandHandle add_command(const CommandSpec& spec);

    // Names the struct this workbench keeps between runs (see WorkbenchState.h); call once from the constructor or declare_panels. `version` rises only when a field's meaning changes.
    template<typename S>
    void persist(S& state, uint32_t version = 1)
    {
        m_binding = create_unique<Binding<S>>(state, version);
    }

private:
    class IBinding
    {
    public:
        virtual ~IBinding() = default;
        virtual std::string write() const = 0;
        virtual void read(std::string_view yaml) = 0;
        virtual uint64_t hash() const = 0;
        [[nodiscard]] virtual uint32_t version() const = 0;
    };

    template<typename S>
    class Binding final : public IBinding
    {
    public:
        Binding(S& state, uint32_t version)
            : m_state(state)
            , m_version(version)
        {
        }

        std::string write() const override
        {
            StateWriter writer(m_version);
            reflect(writer, m_state);
            return writer.text();
        }

        void read(std::string_view yaml) override
        {
            StateReader reader(yaml, m_version);
            reflect(reader, m_state);
        }

        uint64_t hash() const override
        {
            StateHash hasher;
            reflect(hasher, m_state);
            return hasher.value();
        }

        [[nodiscard]] uint32_t version() const override { return m_version; }

    private:
        S& m_state;
        uint32_t m_version;
    };

    struct PanelEntry
    {
        std::string id;
    };

    void route_shortcuts();
    void restore_state();
    void save_state();

    std::string m_name;
    std::string m_state_key;
    WorkbenchOptions m_options;
    WorkbenchContext* m_context = nullptr;
    bool m_attached = false;
    bool m_was_open = false;
    bool m_state_restored = false;
    uint64_t m_state_hash = 0;
    PanelEntry m_panels[k_max_workbench_panels];
    uint32_t m_panel_count = 0;
    CommandInfo m_commands[k_max_workbench_commands];
    uint32_t m_command_count = 0;
    UniquePtr<IBinding> m_binding;
};

} // namespace oryx
