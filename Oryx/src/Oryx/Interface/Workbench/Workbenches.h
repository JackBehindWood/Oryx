#pragma once

#include "Oryx/Debug/ConsoleCommands.h"
#include "Oryx/Interface/Workbench/CommandPalette.h"
#include "Oryx/Interface/Workbench/Workbench.h"

namespace oryx
{

// Owns a set of workbenches and drives them in order. Everything but destruction needs the app's GuiContext active (a ContextScope<GuiContext>).
//   Dashboard& dashboard = workbenches.add<Dashboard>();
//   workbenches.update(frame);  // each frame, inside begin_frame/end_frame, before drawing
//   workbenches.draw();         // inside the panel host
class Workbenches
{
public:
    Workbenches();
    ~Workbenches();
    Workbenches(const Workbenches&) = delete;
    Workbenches& operator=(const Workbenches&) = delete;

    // Constructs the workbench in place, attaches it (registering its panels) and returns it; a throw from attach leaves nothing added.
    template<typename T, typename... Args>
    T& add(Args&&... args)
    {
        static_assert(std::is_base_of_v<IWorkbench, T>, "a workbench implements IWorkbench");
        UniquePtr<T> workbench = create_unique<T>(std::forward<Args>(args)...);
        T& reference = *workbench;
        attach(std::move(workbench));
        return reference;
    }

    template<typename T>
    [[nodiscard]] T* find()
    {
        for (const UniquePtr<IWorkbench>& workbench : m_items)
        {
            if (T* match = dynamic_cast<T*>(workbench.get()))
            {
                return match;
            }
        }
        return nullptr;
    }

    // Throws Error when there is no such workbench.
    template<typename T>
    [[nodiscard]] T& require()
    {
        T* match = find<T>();
        if (match == nullptr)
        {
            throw Error(std::string("no workbench of type ") + typeid(T).name());
        }
        return *match;
    }

    [[nodiscard]] size_t size() const { return m_items.size(); }
    [[nodiscard]] IWorkbench* find_named(std::string_view name);

    void update(const WorkbenchFrame& frame);
    void draw();
    // Each workbench's contributions, one after another: the items of the menu being built, the status bar's items.
    void menu();
    void status();
    void toolbar();
    // Opens the command palette (also Cmd/Ctrl+Shift+P, read in update): every workbench command and every console command, run by name when chosen.
    void open_palette();
    // Draws the palette while it is open and runs the command it returns; call once per frame inside the GUI frame, outside any panel.
    void overlay();
    // A "Window" menu: one check item per workbench that opens and closes it, then the dock's panel list and layout history.
    void window_menu();

    // The commands of every workbench as `<workbench>.<command>` for the console.
    [[nodiscard]] IConsoleSource& commands() { return m_commands; }

private:
    class CommandSource final : public IConsoleSource
    {
    public:
        explicit CommandSource(Workbenches& owner)
            : m_owner(owner)
        {
        }

        bool execute(ConsoleCall& call) override;
        void names(std::vector<std::string>& out) const override;

    private:
        Workbenches& m_owner;
    };

    void attach(UniquePtr<IWorkbench> workbench);

    std::vector<UniquePtr<IWorkbench>> m_items;
    WorkbenchContext m_context;
    CommandSource m_commands;
    CommandPalette m_palette;
};

} // namespace oryx
