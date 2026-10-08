#include "OasisShell.h"

#ifdef OX_ENABLE_GRAPHICS

namespace oasis
{

namespace
{

using namespace oryx;

constexpr const char* k_confirm = "Switch match?";

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
    , m_dashboard_open(flag != DashboardFlag::Default ? flag == DashboardFlag::On : settings_of<DashboardSettings>().enabled)
{
    m_model.feed = &m_feed;
    m_panel = make_panel_state(m_settings.views);
    m_settings_subscription = on_settings_changed<DashboardSettings>(
        [this](const DashboardSettings& changed)
        {
            m_settings = changed;
            m_settings_dirty = true;
        });
}

OasisShell::~OasisShell() noexcept = default;

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
        m_panel = make_panel_state(m_settings.views);
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
    if (m_showcase_open)
    {
        m_showcase.set_region(m_context.workspace().right);
        m_showcase.set_input_blocked(m_context.popup_open());
        m_showcase.frame(info);
        claim_input();
    }
    Renderer::scene().submit(*this);
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
    const Rect central = m_context.workspace().central;
    if (!is_empty(central))
    {
        m_router.set_view(k_main_view, ViewRegion{ central.min, central.size });
    }
    claim_input();
}

void OasisShell::claim_input()
{
    const bool showcase = m_showcase_open;
    const GuiContext& other = m_showcase.context();
    if (m_context.wants_mouse() || m_context.popup_open() || (showcase && (other.wants_mouse() || other.popup_open())))
    {
        m_router.claim_pointer();
    }
    if (m_context.wants_keyboard() || m_context.popup_open() || (showcase && (other.wants_keyboard() || other.popup_open())))
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
    gui::central_area();
    if (m_dashboard_open || m_showcase_open)
    {
        const float width = m_showcase_open ? m_window[0] : static_cast<float>(m_settings.panel_width);
        ImStyle hosted = gui::theme().panel;
        hosted.background.a = 0.0f;
        hosted.border_width = 0.0f;
        gui::WidgetOptions options;
        options.style = m_showcase_open ? &hosted : nullptr;
        gui::begin_side_panel(gui::Side::Right, width, options);
        if (!m_showcase_open)
        {
            draw_dashboard(m_panel, m_model);
        }
        gui::end_side_panel();
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
    if (gui::begin_menu("View"))
    {
        if (gui::menu_item("Dashboard", m_dashboard_open && !m_showcase_open).clicked)
        {
            m_dashboard_open = !m_dashboard_open || m_showcase_open;
            m_showcase_open = false;
        }
        gui::end_menu();
    }
    if (gui::begin_menu("Help"))
    {
        if (gui::menu_item("GUI Showcase", m_showcase_open).clicked)
        {
            m_showcase_open = !m_showcase_open;
        }
        gui::end_menu();
    }
    gui::end_menu_bar();
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
