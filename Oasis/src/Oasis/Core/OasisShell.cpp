#include "OasisShell.h"

#ifdef OX_ENABLE_GRAPHICS

namespace oasis
{

namespace
{

using namespace oryx;

constexpr const char* k_confirm = "Switch match?";
constexpr const char* k_showcase_panel = "view/gui-showcase";
constexpr float k_showcase_min_width = 320.0f;
constexpr float k_showcase_min_height = 240.0f;

oryx::FeedOptions feed_options(const DashboardSettings& settings)
{
    FeedOptions options;
    options.capacity = settings.history;
    return options;
}

} // namespace

OasisShell::OasisShell(InputRouter& router, DashboardFlag flag)
    : m_router(router)
    , m_feed(feed_options(settings_of<DashboardSettings>()))
    , m_settings(settings_of<DashboardSettings>())
{
    m_model.feed = &m_feed;
    m_panel = make_panel_state(m_settings.views);
    m_settings_subscription = on_settings_changed<DashboardSettings>(
        [this](const DashboardSettings& changed)
        {
            m_settings = changed;
            m_settings_dirty = true;
        });

    ContextScope<GuiContext> scope(m_context);
    register_dashboard_panels(m_panel, m_settings.enabled);
    gui::PanelOptions showcase;
    showcase.title = "GUI Showcase";
    showcase.min_w = k_showcase_min_width;
    showcase.min_h = k_showcase_min_height;
    showcase.foreign_body = true;
    showcase.initial_open = false;
    showcase.dock_tabbed_with = m_panel.panels.empty() ? std::string_view() : std::string_view(m_panel.panels[0]);
    showcase.dock_near = k_viewport_panel;
    static_cast<void>(gui::register_panel(k_showcase_panel, showcase));

    if (flag != DashboardFlag::Default)
    {
        static_cast<void>(gui::set_group_open(k_dashboard_group, flag == DashboardFlag::On, gui::GroupEdit::SessionOnly));
    }
}

OasisShell::~OasisShell() noexcept = default;

const gui::DockLayout& OasisShell::layout()
{
    ContextScope<GuiContext> scope(m_context);
    return gui::dock_layout();
}

bool OasisShell::dashboard_open()
{
    ContextScope<GuiContext> scope(m_context);
    return gui::is_group_open(k_dashboard_group);
}

bool OasisShell::showcase_open()
{
    ContextScope<GuiContext> scope(m_context);
    return gui::is_panel_open(k_showcase_panel);
}

void OasisShell::set_font(Font* font)
{
    GuiTheme theme = m_context.gui_theme();
    theme.font = font;
    m_context.set_theme(theme);
    m_showcase.set_font(font);
}

void OasisShell::apply_settings()
{
    if (m_settings_dirty)
    {
        m_settings_dirty = false;
        DashboardPanelState fresh = make_panel_state(m_settings.views);
        ContextScope<GuiContext> scope(m_context);
        register_dashboard_panels(fresh, m_settings.enabled);
        m_panel = std::move(fresh);
    }
}

void OasisShell::frame(const FrameInfo& info)
{
    if (m_context.gui_theme().font == nullptr)
    {
        set_font(&Renderer::default_font());
    }
    m_window = info.logical;
    run(make_im_input(info.input, 0, info.logical, info.scale, static_cast<float>(info.delta_time)));

    if (Window* window = Application::Get().window())
    {
        window->set_cursor_kind(static_cast<CursorKind>(m_context.output().cursor));
        if (!m_context.output().copy_text.empty())
        {
            window->set_clipboard_text(m_context.output().copy_text);
        }
    }
    draw_showcase(info);
    Renderer::scene().submit(*this);
}

void OasisShell::draw_showcase(const FrameInfo& info)
{
    ContextScope<GuiContext> scope(m_context);
    const Rect region = gui::panel_rect(k_showcase_panel);
    if (is_empty(region))
    {
        return;
    }
    m_showcase.set_region(region);
    m_showcase.set_input_blocked(gui::panel_blocked(k_showcase_panel));
    m_showcase.frame(info);
    claim_input();
}

void OasisShell::run(const ImInput& input)
{
    apply_settings();
    ContextScope<GuiContext> scope(m_context);
    try
    {
        m_context.begin_frame(input);
        build();
        m_context.end_frame();
    }
    catch (...)
    {
        m_context.abort_frame();
        throw;
    }
    const Rect board = gui::viewport_rect(k_viewport_panel);
    if (!is_empty(board))
    {
        m_router.set_view(k_main_view, ViewRegion{ board.min, board.size });
    }
    claim_input();
}

void OasisShell::claim_input()
{
    ContextScope<GuiContext> scope(m_context);
    const GuiContext& other = m_showcase.context();
    if (m_context.wants_mouse() || m_context.popup_open() || gui::panel_claims_pointer(k_showcase_panel, other))
    {
        m_router.claim_pointer();
    }
    if (m_context.wants_keyboard() || m_context.popup_open() || gui::panel_claims_keyboard(k_showcase_panel, other))
    {
        m_router.claim_keyboard();
    }
}

void OasisShell::render_stage(RenderStage stage, StageContext& context)
{
    if (stage != RenderStage::Overlay || m_context.gui_theme().font == nullptr)
    {
        return;
    }
    replay(m_context.draw_list(), context.batcher_2d, *m_context.gui_theme().font, ReplayTarget{ { 0.0f, 0.0f }, m_window });
}

void OasisShell::build()
{
    gui::begin_workspace("shell");
    menus();
    gui::begin_body();
    {
        gui::PanelHostScope host;
        for (size_t index = 0; index < m_panel.views.size(); ++index)
        {
            draw_dashboard_panel(m_panel, index, m_model);
        }
        static_cast<void>(gui::begin_panel(k_showcase_panel));
        gui::end_panel();
    }
    gui::end_body();
    gui::end_workspace();
    answer_switch();
}

void OasisShell::menus()
{
    gui::begin_menu_bar("menu");
    game_menu();
    opponent_menu();
    view_menu();
    if (gui::begin_menu("Help"))
    {
        if (gui::menu_item("GUI Showcase", gui::is_panel_open(k_showcase_panel)).clicked)
        {
            static_cast<void>(gui::toggle_panel(k_showcase_panel));
        }
        gui::end_menu();
    }
    gui::end_menu_bar();
}

void OasisShell::view_menu()
{
    if (!gui::begin_menu("View"))
    {
        return;
    }
    if (gui::menu_item("Dashboard", gui::is_group_open(k_dashboard_group)).clicked)
    {
        static_cast<void>(gui::set_group_open(k_dashboard_group, !gui::is_group_open(k_dashboard_group)));
    }
    gui::dock_menu();
    gui::end_menu();
}

void OasisShell::game_menu()
{
    if (!gui::begin_menu("Game"))
    {
        return;
    }
    for (const std::string& name : selection::creatable_games())
    {
        if (gui::menu_item(name, m_board != nullptr && name == m_board->game_name()).clicked)
        {
            request_switch(name, std::string());
        }
    }
    gui::end_menu();
}

void OasisShell::opponent_menu()
{
    if (!gui::begin_menu("Opponent"))
    {
        return;
    }
    if (m_board != nullptr)
    {
        for (const std::string& name : selection::opponents_for(m_board->game_name()))
        {
            if (gui::menu_item(name, name == m_board->opponent_name()).clicked)
            {
                request_switch(std::string(), name);
            }
        }
    }
    gui::end_menu();
}

void OasisShell::request_switch(const std::string& game, const std::string& opponent)
{
    if (m_board == nullptr || ((game.empty() || game == m_board->game_name()) && (opponent.empty() || opponent == m_board->opponent_name())))
    {
        return;
    }
    m_pending_game = game;
    m_pending_opponent = opponent;
    if (!m_board->match_in_progress())
    {
        dispatch_switch();
        return;
    }
    m_switch_pending = true;
    m_confirm_requested = true;
}

void OasisShell::answer_switch()
{
    if (!m_switch_pending)
    {
        return;
    }
    if (m_confirm_requested)
    {
        // Ids are scoped, so the modal opens here at the top level rather than inside the menu that asked.
        m_confirm_requested = false;
        gui::open_modal(k_confirm);
    }
    const std::string& target = m_pending_game.empty() ? m_pending_opponent : m_pending_game;
    const gui::ModalChoice choice = gui::confirm(k_confirm, m_context.arena().format("Switching to %s ends the current match and its progress is lost.", target.c_str()), "Switch", "Cancel");
    if (choice == gui::ModalChoice::None)
    {
        return;
    }
    m_switch_pending = false;
    if (choice == gui::ModalChoice::Confirmed)
    {
        dispatch_switch();
    }
}

void OasisShell::dispatch_switch()
{
    m_feed.clear_at_next_match();
    StartMatchEvent request(m_pending_game, m_pending_opponent);
    Application::Get().post_event(request);
}

} // namespace oasis

#endif
