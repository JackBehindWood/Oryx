#include "oxpch.h"
#include "Oryx/Interface/Workbench/Workbenches.h"

namespace oryx
{

Workbenches::Workbenches()
    : m_context{ *this }
    , m_commands(*this)
{
}

Workbenches::~Workbenches()
{
    for (size_t i = m_items.size(); i > 0; --i)
    {
        m_items[i - 1]->detach();
    }
}

void Workbenches::attach(UniquePtr<IWorkbench> workbench)
{
    if (find_named(workbench->name()) != nullptr)
    {
        throw Error("a workbench named '" + std::string(workbench->name()) + "' is already added");
    }
    workbench->attach(m_context);
    m_items.push_back(std::move(workbench));
}

IWorkbench* Workbenches::find_named(std::string_view name)
{
    for (const UniquePtr<IWorkbench>& workbench : m_items)
    {
        if (workbench->name() == name)
        {
            return workbench.get();
        }
    }
    return nullptr;
}

void Workbenches::update(const WorkbenchFrame& frame)
{
    for (const UniquePtr<IWorkbench>& workbench : m_items)
    {
        workbench->update(frame);
    }
}

void Workbenches::draw()
{
    for (const UniquePtr<IWorkbench>& workbench : m_items)
    {
        workbench->draw();
    }
}

void Workbenches::menu()
{
    for (const UniquePtr<IWorkbench>& workbench : m_items)
    {
        workbench->menu();
    }
}

void Workbenches::status()
{
    for (const UniquePtr<IWorkbench>& workbench : m_items)
    {
        workbench->status();
    }
}

void Workbenches::toolbar()
{
    for (const UniquePtr<IWorkbench>& workbench : m_items)
    {
        workbench->toolbar();
    }
}

void Workbenches::window_menu()
{
    if (!gui::begin_menu("Window"))
    {
        return;
    }
    for (const UniquePtr<IWorkbench>& workbench : m_items)
    {
        const std::string_view name = workbench->name();
        const bool open = gui::is_group_open(name);
        std::string title(name);
        if (!title.empty())
        {
            title[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(title[0])));
        }
        if (gui::menu_item(title, open).clicked)
        {
            static_cast<void>(gui::set_group_open(name, !open));
        }
    }
    gui::separator();
    gui::dock_menu();
    gui::end_menu();
}

bool Workbenches::CommandSource::execute(ConsoleCall& call)
{
    const std::string_view full = call.name();
    const size_t dot = full.find('.');
    if (dot == std::string_view::npos)
    {
        return false;
    }
    IWorkbench* workbench = m_owner.find_named(full.substr(0, dot));
    if (workbench == nullptr)
    {
        return false;
    }
    const std::string_view id = full.substr(dot + 1);
    for (uint32_t i = 0; i < workbench->command_count(); ++i)
    {
        const CommandInfo& info = workbench->command_info(CommandHandle{ i });
        if (info.id != id)
        {
            continue;
        }
        if (!info.enabled)
        {
            throw Error("command '" + std::string(full) + "' is disabled right now");
        }
        std::vector<std::string_view> words;
        for (size_t arg = 0; arg < call.count(); ++arg)
        {
            words.push_back(call.arg(arg));
        }
        workbench->command(CommandHandle{ i }, CommandArgs{ words.data(), static_cast<uint32_t>(words.size()) });
        return true;
    }
    return false;
}

void Workbenches::CommandSource::names(std::vector<std::string>& out) const
{
    for (const UniquePtr<IWorkbench>& workbench : m_owner.m_items)
    {
        for (uint32_t i = 0; i < workbench->command_count(); ++i)
        {
            out.push_back(std::string(workbench->name()) + "." + workbench->command_info(CommandHandle{ i }).id);
        }
    }
}

} // namespace oryx
