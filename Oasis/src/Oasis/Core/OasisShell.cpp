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

oryx::AnnotationFlags flags_of(const DashboardSettings& settings)
{
    AnnotationFlags flags;
    for (const std::string& name : settings.annotations)
    {
        flags.heat = flags.heat || name == "heat";
        flags.chosen = flags.chosen || name == "chosen";
        flags.value = flags.value || name == "value";
    }
    return flags;
}

oryx::FeedOptions feed_options(const DashboardSettings& settings)
{
    FeedOptions options;
    options.capacity = settings.history;
    options.max_tree_nodes = settings.max_tree_nodes;
    return options;
}

} // namespace

OasisShell::OasisShell(InputRouter& router, DashboardFlag flag)
    : m_router(router)
    , m_feed(feed_options(settings_of<DashboardSettings>()))
    , m_observer(m_feed)
    , m_settings(settings_of<DashboardSettings>())
{
    m_model.feed = &m_feed;
    m_flags = flags_of(m_settings);
    m_pushed_flags = m_flags;
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

void OasisShell::board_created(IGraphicsBoard& board, const std::string& game)
{
    m_graphics_board = &board;
    m_pushed_flags = board.annotation_flags();
    m_feed.set_describer(make_presenter_describer(game));
    m_model.pinned_action = INVALID_ACTION;
    m_model.board_hovered_action = INVALID_ACTION;
    m_annotations.items.clear();
}

void OasisShell::board_released(IGraphicsBoard& board)
{
    if (m_graphics_board == &board)
    {
        m_graphics_board = nullptr;
    }
}

void OasisShell::sync_board()
{
    if (m_graphics_board == nullptr)
    {
        return;
    }
    const AnnotationFlags on_board = m_graphics_board->annotation_flags();
    if (!(on_board == m_pushed_flags))
    {
        m_flags = on_board;
    }
    const bool enabled = dashboard_open();
    if (m_feed.match_index() != m_seen_match)
    {
        m_seen_match = m_feed.match_index();
        m_model.pinned_action = INVALID_ACTION;
        follow_latest_record(m_model);
    }
    sanitise_selection(m_model);
    RecordView selected = selected_view(m_model);
    // A restart leaves the last decision of the finished match as the newest record; nothing of it belongs on the fresh board.
    if (selected.valid && selected.record->match_index != m_feed.match_index())
    {
        selected = RecordView{};
    }
    annotate_decision(selected, m_annotations);
    const bool ghost = enabled && selected.valid && m_feed.size() > 0 && selected.record->sequence != m_feed.view(m_feed.size() - 1).record->sequence;
    m_legend.clear();
    if (enabled && selected.valid)
    {
        char text[96];
        std::snprintf(text, sizeof(text), "Ply %u \xC2\xB7 Player %d \xC2\xB7 %s", selected.record->decision_index + 1, static_cast<int32_t>(selected.record->player) + 1, m_board != nullptr ? m_board->opponent_name().c_str() : "");
        m_legend = text;
    }
    m_model.board_hovered_action = enabled ? action_ending_at(selected, m_graphics_board->hovered_space()) : INVALID_ACTION;
    BoardAnnotationRequest request;
    request.annotations = enabled ? &m_annotations : nullptr;
    request.flags = enabled ? m_flags : AnnotationFlags{};
    request.controls = enabled;
    request.legend = m_legend;
    request.ghost = ghost;
    request.linked = enabled ? action_destination(selected, linked_action(m_model)) : k_no_space;
    m_graphics_board->set_annotations(request);
    m_pushed_flags = request.flags;
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
        m_flags = flags_of(m_settings);
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
    sync_board();

    if (Window* window = Application::Get().window())
    {
        window->set_cursor_kind(static_cast<CursorKind>(m_context.output().cursor));
        if (!m_context.output().copy_text.empty())
        {
            window->set_clipboard_text(m_context.output().copy_text);
        }
    }
    if (showcase_floating())
    {
        Renderer::scene().submit(*this);
        draw_showcase(info);
    }
    else
    {
        draw_showcase(info);
        Renderer::scene().submit(*this);
    }
}

bool OasisShell::showcase_floating()
{
    ContextScope<GuiContext> scope(m_context);
    return gui::is_panel_floating(k_showcase_panel);
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
    m_model.hovered_action = INVALID_ACTION;
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
    annotation_menu();
    if (gui::menu_item("Synthetic search tree", m_observer.enabled()).clicked)
    {
        m_observer.set_enabled(!m_observer.enabled());
    }
    gui::dock_menu();
    gui::end_menu();
}

void OasisShell::annotation_menu()
{
    gui::separator();
    if (gui::menu_item("Board heat", m_flags.heat).clicked)
    {
        m_flags.heat = !m_flags.heat;
    }
    if (gui::menu_item("Chosen move", m_flags.chosen).clicked)
    {
        m_flags.chosen = !m_flags.chosen;
    }
    if (gui::menu_item("Move values", m_flags.value).clicked)
    {
        m_flags.value = !m_flags.value;
    }
    gui::separator();
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
