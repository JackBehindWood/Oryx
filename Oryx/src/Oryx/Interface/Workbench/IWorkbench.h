#pragma once

#include "Oryx/Interface/Workbench/WorkbenchCommand.h"

namespace oryx
{

class Workbenches;

// What a workbench is attached into.
struct WorkbenchContext
{
    Workbenches& workbenches;
};

struct WorkbenchFrame
{
    float delta_time = 0.0f;
    uint64_t frame = 0;
};

// A named set of dockable panels with a lifecycle: the dashboard is one, a future editor another. The contract is pure so an app can implement it directly; Workbench is the base with the usual behaviour.
// Everything runs on the active GuiContext, between begin_frame and end_frame; attach needs one active to register panels.
class IWorkbench
{
public:
    virtual ~IWorkbench() = default;

    [[nodiscard]] virtual std::string_view name() const = 0;
    virtual void attach(WorkbenchContext& context) = 0;
    virtual void detach() = 0;
    // Once per frame before any drawing.
    virtual void update(const WorkbenchFrame& frame) = 0;
    // Inside the panel host: begin_panel / end_panel for each of its panels.
    virtual void draw() = 0;
    // Contributions drawn into the app's Areas: items of the menu being built, of the status bar and of a toolbar.
    virtual void menu() = 0;
    virtual void status() = 0;
    virtual void toolbar() = 0;

    [[nodiscard]] virtual uint32_t command_count() const = 0;
    [[nodiscard]] virtual const CommandInfo& command_info(CommandHandle command) const = 0;
    virtual void command(CommandHandle command, const CommandArgs& args) = 0;
};

} // namespace oryx
