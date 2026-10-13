#include "doctest.h"

#include "Oryx.h"
#include "Oryx/Interface/Workbench/Workbenches.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

struct NotesState
{
    int32_t level = 0;
    std::string search;
    bool pinned = false;
};

template<typename Visitor>
void reflect(Visitor& v, NotesState& s)
{
    v.field("level", s.level);
    v.field("search", s.search);
    v.field("pinned", s.pinned);
}

class Notes : public Workbench
{
public:
    explicit Notes(const WorkbenchOptions& options = {})
        : Workbench("notes", options)
    {
        persist(m_state);
    }

    void declare_panels() override
    {
        m_list = add_panel("list", "List");
        m_detail = add_panel("detail", "Detail");
        m_save = add_command({ "save", "Save", { ImKey::C, true, false, false }, false });
        m_ping = add_command({ "ping", "Ping", { ImKey::V, true, false, false }, true });
    }

    void draw_panel(PanelHandle panel, const PanelFrame& frame) override
    {
        ++drawn[panel.index];
        keyboard_ok = frame.keyboard_ok;
        gui::label("notes");
    }

    void command(CommandHandle handle, const CommandArgs& args) override
    {
        last = handle;
        ++commands;
        last_args = args.count;
    }

    void opened() override { ++opens; }
    void closed() override { ++closes; }
    void tick(const WorkbenchFrame&) override { ++ticks; }
    void state_loaded() override { ++loads; }

    PanelHandle m_list;
    PanelHandle m_detail;
    CommandHandle m_save;
    CommandHandle m_ping;
    NotesState m_state;
    uint32_t drawn[2] = {};
    uint32_t opens = 0;
    uint32_t closes = 0;
    uint32_t ticks = 0;
    uint32_t loads = 0;
    uint32_t commands = 0;
    uint32_t last_args = 0;
    bool keyboard_ok = false;
    CommandHandle last;
};

struct Rig
{
    GuiFixture f;
    Workbenches workbenches;

    template<typename T, typename... Args>
    T& add(Args&&... args)
    {
        return workbenches.add<T>(std::forward<Args>(args)...);
    }

    void frame()
    {
        f.driver.frame(f.frame_of(
            [this]
            {
                workbenches.update(WorkbenchFrame{ 1.0f / 60.0f, 0 });
                gui::PanelHostScope host;
                workbenches.draw();
            }));
    }

    void frames(uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            frame();
        }
    }
};

} // namespace

TEST_CASE("workbench: attach registers the panels in the workbench's group, once")
{
    Rig rig;
    Notes& notes = rig.add<Notes>();
    CHECK(gui::panels().count == 2);
    CHECK(notes.panel_name(notes.m_list) == "notes/list");
    CHECK(notes.panel_name(notes.m_detail) == "notes/detail");
    CHECK(notes.command_count() == 2);

    WorkbenchContext context{ rig.workbenches };
    notes.attach(context);
    CHECK(gui::panels().count == 2);
    CHECK_THROWS_AS(rig.add<Notes>(), Error);
}

TEST_CASE("workbench: find and require look workbenches up by type")
{
    Rig rig;
    CHECK(rig.workbenches.find<Notes>() == nullptr);
    CHECK_THROWS_AS(static_cast<void>(rig.workbenches.require<Notes>()), Error);
    Notes& notes = rig.add<Notes>();
    CHECK(rig.workbenches.find<Notes>() == &notes);
    CHECK(&rig.workbenches.require<Notes>() == &notes);
    CHECK(rig.workbenches.find_named("notes") == &notes);
}

TEST_CASE("workbench: open panels are drawn through draw_panel")
{
    Rig rig;
    Notes& notes = rig.add<Notes>();
    rig.frames(4);
    CHECK(notes.open());
    CHECK(notes.drawn[0] + notes.drawn[1] > 0);
    static_cast<void>(gui::select_panel("notes/list"));
    rig.frames(3);
    CHECK(notes.drawn[0] > 0);
    static_cast<void>(gui::select_panel("notes/detail"));
    rig.frames(3);
    CHECK(notes.drawn[1] > 0);
}

TEST_CASE("workbench: closing the group runs closed once and stops the updates; always_update keeps them")
{
    Rig rig;
    Notes& notes = rig.add<Notes>();
    rig.frames(3);
    CHECK(notes.opens == 1);
    const uint32_t ticks_open = notes.ticks;
    CHECK(ticks_open == 3);

    notes.set_open(false);
    rig.frames(3);
    CHECK(notes.closes == 1);
    CHECK(notes.ticks == ticks_open);

    notes.set_open(true);
    rig.frames(2);
    CHECK(notes.opens == 2);
    CHECK(notes.ticks == ticks_open + 2);

    Rig counting;
    WorkbenchOptions options;
    options.always_update = true;
    Notes& always = counting.add<Notes>(options);
    always.set_open(false);
    counting.frames(3);
    CHECK(always.ticks == 3);
}

TEST_CASE("workbench: a shortcut runs its command only with a panel focused, a global one always, a disabled one never")
{
    Rig rig;
    Notes& notes = rig.add<Notes>();
    rig.frames(3);

    rig.f.driver.chord(ImKey::C);
    rig.frame();
    CHECK(notes.commands == 0);

    gui::focus_panel("notes/list");
    rig.frames(2);
    rig.f.driver.chord(ImKey::C);
    rig.frame();
    CHECK(notes.commands == 1);
    CHECK(notes.last == notes.m_save);

    gui::focus_panel("");
    rig.frames(2);
    rig.f.driver.chord(ImKey::V);
    rig.frame();
    CHECK(notes.commands == 2);
    CHECK(notes.last == notes.m_ping);

    notes.set_command_enabled(notes.m_ping, false);
    rig.f.driver.chord(ImKey::V);
    rig.frame();
    CHECK(notes.commands == 2);
    CHECK_THROWS_AS(notes.set_command_enabled(CommandHandle{ 9 }, true), Error);
}

TEST_CASE("workbench: a shortcut with the wrong modifiers does nothing")
{
    Shortcut save{ ImKey::C, true, false, false };
    ImKeys keys;
    keys.pressed = im_key_bit(ImKey::C);
    keys.shortcut = true;
    CHECK(shortcut_pressed(save, keys));
    keys.shift = true;
    CHECK_FALSE(shortcut_pressed(save, keys));
    keys.shift = false;
    keys.shortcut = false;
    CHECK_FALSE(shortcut_pressed(save, keys));
    CHECK_FALSE(shortcut_pressed(Shortcut{}, keys));
    CHECK(shortcut_text(Shortcut{ ImKey::C, true, true, false }).find("Shift+C") != std::string::npos);
    CHECK(shortcut_text(Shortcut{}).empty());
}

TEST_CASE("workbench: the console sees commands as workbench.command")
{
    Rig rig;
    Notes& notes = rig.add<Notes>();
    std::vector<std::string> names;
    rig.workbenches.commands().names(names);
    CHECK(names == std::vector<std::string>{ "notes.save", "notes.ping" });
#ifdef OX_ENABLE_CONSOLE_COMMANDS
    const ConsoleResult ran = console::run("notes.ping a b", &rig.workbenches.commands());
    CHECK(ran.ok);
    CHECK(notes.commands == 1);
    CHECK(notes.last == notes.m_ping);
    CHECK(notes.last_args == 2);
    notes.set_command_enabled(notes.m_ping, false);
    CHECK_FALSE(console::run("notes.ping", &rig.workbenches.commands()).ok);
    CHECK_FALSE(console::run("notes.missing", &rig.workbenches.commands()).ok);
#endif
}

TEST_CASE("workbench: state changes land in the layout's extension and are read back by the next workbench")
{
    std::string saved;
    {
        Rig rig;
        Notes& notes = rig.add<Notes>();
        rig.frames(2);
        notes.m_state.level = 3;
        notes.m_state.search = "abc";
        notes.m_state.pinned = true;
        rig.frames(2);
        saved = std::string(gui::layout_extension("workbench.notes"));
        CHECK(saved.find("level: 3") != std::string::npos);
        CHECK(saved.find("search: abc") != std::string::npos);
        CHECK(notes.loads == 0);
    }
    Rig second;
    Notes& notes = second.add<Notes>();
    gui::set_layout_extension("workbench.notes", saved);
    second.frames(2);
    CHECK(notes.loads == 1);
    CHECK(notes.m_state.level == 3);
    CHECK(notes.m_state.search == "abc");
    CHECK(notes.m_state.pinned);
}

TEST_CASE("workbench: a corrupt, newer or mistyped state keeps the defaults")
{
    for (const char* text : { "{{{ not yaml", "version: 9\nlevel: 4\n", "version: 1\nlevel: banana\nsearch: kept\n", "- a list" })
    {
        Rig rig;
        Notes& notes = rig.add<Notes>();
        gui::set_layout_extension("workbench.notes", text);
        rig.frames(2);
        CHECK(notes.m_state.level == 0);
        if (std::string_view(text).find("kept") == std::string_view::npos)
            CHECK(notes.m_state.search.empty());
        else
            CHECK(notes.m_state.search == "kept");
    }
}

TEST_CASE("workbench: a warm frame allocates nothing")
{
    Rig rig;
    Notes& notes = rig.add<Notes>();
    notes.m_state.level = 1;
    rig.frames(6);
    const MemoryStats before = all_allocations();
    rig.frames(3);
    const MemoryStats after = all_allocations();
    CHECK(after.allocation_count == before.allocation_count);
}

TEST_CASE("workbench: group rules apply on attach")
{
    Rig rig;
    WorkbenchOptions options;
    options.rules = gui::rules_own_group(gui::group("notes"));
    CHECK_NOTHROW(static_cast<void>(rig.add<Notes>(options)));
}
