#include "OasisLayer.h"

#include "OasisMenus.h"
#include "SimulationRequest.h"
#include "SyntheticTreeObserver.h"

#include "Oryx/Benchmark/BenchmarkReport.h"
#include "Oryx/Events/SimulationEvent.h"
#ifdef OX_ENABLE_GRAPHICS
#include "Oryx/Assets/GpuShaderStore.h"
#endif

namespace oasis
{

namespace
{

#ifdef OX_ENABLE_GRAPHICS
constexpr bool k_graphics_built = true;
constexpr const char* k_showcase_panel = "tools/gui-showcase";
constexpr float k_showcase_width = 360.0f;
constexpr float k_console_height = 180.0f;
constexpr float k_console_min_height = 120.0f;
constexpr float k_showcase_min_width = 320.0f;
constexpr float k_showcase_min_height = 240.0f;
#else
constexpr bool k_graphics_built = false;
#endif

} // namespace

#ifdef OX_ENABLE_GRAPHICS

struct OasisLayer::Chrome
{
    explicit Chrome(oryx::InputRouter& input_router)
        : router(input_router)
    {
        oryx::ContextScope<oryx::GuiContext> scope(context);
        dashboard = &workbenches.add<oryx::Dashboard>();
        for (const char* view : { "trace", "decision", "stats", "search" })
        {
            dashboard->add_view(view);
        }
        oryx::gui::PanelOptions showcase;
        showcase.title = "GUI Showcase";
        showcase.min_w = k_showcase_min_width;
        showcase.min_h = k_showcase_min_height;
        showcase.group = "tools";
        showcase.initial_open = false;
        showcase.dock_near = oryx::k_viewport_panel;
        showcase.dock_size = k_showcase_width;
        if (dashboard->view_count() > 0)
        {
            showcase.dock_tabbed_with = dashboard->panel_name(oryx::PanelHandle{ 0 });
        }
        static_cast<void>(oryx::gui::register_panel(k_showcase_panel, showcase));
        console = &workbenches.add<oryx::ConsoleWorkbench>(console_open);
        static_cast<void>(workbenches.add<oryx::SettingsWorkbench>());
        observer = oryx::create_unique<SyntheticTreeObserver>(dashboard->feed());
    }

    oryx::InputRouter& router;
    oryx::GuiContext context;
    oryx::Workbenches workbenches;
    oryx::Dashboard* dashboard = nullptr;
    oryx::ConsoleWorkbench* console = nullptr;
    oryx::UniquePtr<SyntheticTreeObserver> observer;
    SwitchState switching;
    const oryx::BoardLayer* board = nullptr;
    oryx::Vec2f window{ 0.0f, 0.0f };
    bool console_open = false;
};

#endif

OasisLayer::OasisLayer(Options options)
    : oryx::Layer("OasisLayer")
    , m_options(std::move(options))
{
}

OasisLayer::~OasisLayer() noexcept = default;

void OasisLayer::attach()
{
    LaunchPlan plan;
    if (!plan_launch(m_options, k_graphics_built, plan))
    {
        oryx::Application::Get().close(1);
        return;
    }

    if (plan.mode == LaunchMode::Simulate)
    {
        start_simulation();
        return;
    }

    if (plan.mode == LaunchMode::Graphical && !start_graphics(plan))
    {
        OX_INFO("Graphics unavailable - running in the terminal.");
        m_options.headless = true;
        if (!plan_launch(m_options, k_graphics_built, plan))
        {
            oryx::Application::Get().close(1);
            return;
        }
    }

    if (plan.mode == LaunchMode::Console)
    {
        start_play(plan);
    }
}

void OasisLayer::detach()
{
#ifdef OX_ENABLE_GRAPHICS
    m_chrome.reset();
#endif
}

void OasisLayer::event(oryx::Event& event)
{
    oryx::EventDispatcher dispatcher(event);
    dispatcher.dispatch<oryx::SimulationCompleteEvent>(OX_BIND_EVENT_FN(on_simulation_complete));
}

void OasisLayer::start_simulation()
{
    oryx::StartSimulationEvent request = make_simulation_request(m_options);
    oryx::Application::Get().push_layer<oryx::SimulationLayer>(request.benchmark());
    oryx::Application::Get().post_event(request);
}

void OasisLayer::start_play(const LaunchPlan& plan)
{
    oryx::Application& app = oryx::Application::Get();
    app.push_layer<oryx::SimulationLayer>();
    app.push_layer<oryx::BoardLayer>(oryx::BoardLayerDesc{ plan.game, m_options.opponent });
}

bool OasisLayer::start_graphics([[maybe_unused]] const LaunchPlan& plan)
{
#ifdef OX_ENABLE_GRAPHICS
    oryx::Application& app = oryx::Application::Get();
    try
    {
        app.create_window({ "Oasis" });
        oryx::Renderer::init({ .backend = m_options.rhi.empty() ? oryx::default_rhi_backend() : oryx::parse_rhi_backend(m_options.rhi), .shader_store = &oryx::gpu_shader_store() });
    }
    catch (const oryx::Error& error)
    {
        error.log();
        return false;
    }

    oryx::GraphicsLayer& graphics = app.push_overlay<oryx::GraphicsLayer>();
    app.push_layer<oryx::SimulationLayer>();
    use_graphics(graphics.router());
    graphics.add_client(*this, { oryx::FramePhase::Interface });
    oryx::BoardFactory create_board = [&graphics, this](const std::string& game, oryx::PlayerId seat) -> oryx::SharedPtr<oryx::IBoard>
    {
        oryx::SharedPtr<oryx::IGraphicsBoard> board = oryx::create_graphics_board(game, seat);
        if (board != nullptr)
        {
            graphics.add_client(*board);
            board_created(*board, game);
        }
        return board;
    };
    oryx::BoardReleaser release_board = [&graphics, this](oryx::IBoard& board)
    {
        if (oryx::IGraphicsBoard* graphics_board = dynamic_cast<oryx::IGraphicsBoard*>(&board))
        {
            board_released(*graphics_board);
            graphics.remove_client(*graphics_board);
        }
    };
    oryx::BoardLayer& board = app.push_layer<oryx::BoardLayer>(oryx::BoardLayerDesc{ plan.game, m_options.opponent, create_board, false, release_board, &observer() });
    bind(board);
    return true;
#else
    return false;
#endif
}

#ifdef OX_ENABLE_GRAPHICS

void OasisLayer::use_graphics(oryx::InputRouter& router)
{
    m_chrome = oryx::create_unique<Chrome>(router);
}

void OasisLayer::bind(const oryx::BoardLayer& layer)
{
    m_chrome->board = &layer;
    m_chrome->dashboard->bind(layer);
}

void OasisLayer::board_created(oryx::IGraphicsBoard& board, const std::string& game)
{
    if (m_chrome != nullptr)
    {
        m_chrome->dashboard->board_created(board, game);
    }
}

void OasisLayer::board_released(oryx::IGraphicsBoard& board)
{
    if (m_chrome != nullptr)
    {
        m_chrome->dashboard->board_released(board);
    }
}

oryx::DashboardFeed& OasisLayer::feed()
{
    return m_chrome->dashboard->feed();
}

oryx::IDecisionObserver& OasisLayer::observer()
{
    return *m_chrome->observer;
}

oryx::DashboardModel& OasisLayer::model()
{
    return m_chrome->dashboard->model();
}

oryx::Workbenches& OasisLayer::workbenches()
{
    return m_chrome->workbenches;
}

const oryx::GuiContext& OasisLayer::context() const
{
    return m_chrome->context;
}

const oryx::gui::DockLayout& OasisLayer::layout()
{
    oryx::ContextScope<oryx::GuiContext> scope(m_chrome->context);
    return oryx::gui::dock_layout();
}

bool OasisLayer::dashboard_open()
{
    oryx::ContextScope<oryx::GuiContext> scope(m_chrome->context);
    return m_chrome->dashboard->open();
}

bool OasisLayer::showcase_open()
{
    oryx::ContextScope<oryx::GuiContext> scope(m_chrome->context);
    return oryx::gui::is_panel_open(k_showcase_panel);
}

void OasisLayer::set_dashboard_open(bool open)
{
    oryx::ContextScope<oryx::GuiContext> scope(m_chrome->context);
    m_chrome->dashboard->set_open(open);
}

bool OasisLayer::switch_pending() const
{
    return m_chrome->switching.pending;
}

bool OasisLayer::console_open() const
{
    return m_chrome->console_open;
}

const oryx::AnnotationFlags& OasisLayer::annotation_flags() const
{
    return m_chrome->dashboard->annotation_flags();
}

const oryx::BoardAnnotations& OasisLayer::annotations() const
{
    return m_chrome->dashboard->annotations();
}

void OasisLayer::set_font(oryx::Font* font)
{
    oryx::GuiTheme theme = m_chrome->context.gui_theme();
    theme.font = font;
    m_chrome->context.set_theme(theme);
}

void OasisLayer::frame(const oryx::FrameInfo& info)
{
    if (m_chrome->context.gui_theme().font == nullptr)
    {
        set_font(&oryx::Renderer::default_font());
    }
    m_chrome->window = info.logical;
    run(oryx::make_im_input(info.input, 0, info.logical, info.scale, static_cast<float>(info.delta_time)));

    if (oryx::Window* window = oryx::Application::Get().window())
    {
        window->set_cursor_kind(static_cast<oryx::CursorKind>(m_chrome->context.output().cursor));
        if (!m_chrome->context.output().copy_text.empty())
        {
            window->set_clipboard_text(m_chrome->context.output().copy_text);
        }
    }
    oryx::Renderer::scene().submit(*this);
}

void OasisLayer::run(const oryx::ImInput& input)
{
    oryx::ContextScope<oryx::GuiContext> scope(m_chrome->context);
    try
    {
        const oryx::GuiSettings& gui_settings = oryx::settings_of<oryx::GuiSettings>();
        m_chrome->context.set_animations(gui_settings.animations, gui_settings.animation_speed);
        m_chrome->context.begin_frame(input);
        build();
        m_chrome->context.end_frame();
    }
    catch (...)
    {
        m_chrome->context.abort_frame();
        throw;
    }
    const oryx::Rect board = oryx::gui::viewport_rect(oryx::k_viewport_panel);
    if (!oryx::is_empty(board))
    {
        m_chrome->router.set_view(oryx::k_main_view, oryx::ViewRegion{ board.min, board.size });
    }
    claim_input();
}

void OasisLayer::claim_input()
{
    const oryx::GuiContext& context = m_chrome->context;
    if (context.wants_mouse() || context.popup_open())
    {
        m_chrome->router.claim_pointer();
    }
    if (context.wants_keyboard() || context.popup_open())
    {
        m_chrome->router.claim_keyboard();
    }
}

void OasisLayer::render_stage(oryx::RenderStage stage, oryx::StageContext& context)
{
    const oryx::GuiContext& gui_context = m_chrome->context;
    if (stage != oryx::RenderStage::Overlay || gui_context.gui_theme().font == nullptr)
    {
        return;
    }
    replay(gui_context.draw_list(), context.batcher_2d, *gui_context.gui_theme().font, oryx::ReplayTarget{ { 0.0f, 0.0f }, m_chrome->window });
}

void OasisLayer::build()
{
    Chrome& chrome = *m_chrome;
    chrome.workbenches.update(oryx::WorkbenchFrame{ chrome.context.input().delta_time, oryx::gui::io().frame });
    oryx::gui::AreaOptions top;
    top.anchor = oryx::gui::AreaAnchor::Top;
    if (oryx::gui::AreaScope bar("menu_area", top); bar)
    {
        menus();
    }
    oryx::gui::AreaOptions bottom;
    bottom.anchor = oryx::gui::AreaAnchor::Bottom;
    if (oryx::gui::AreaScope bar("status_area", bottom); bar)
    {
        oryx::gui::RowScope row("status", { .width = oryx::grow(), .height = oryx::fixed(oryx::gui::theme().bar_height), .align = oryx::Align::Centre, .gap = 12.0f });
        chrome.workbenches.status();
    }
    oryx::gui::AreaOptions drawer;
    drawer.anchor = oryx::gui::AreaAnchor::Bottom;
    drawer.size = oryx::fixed(k_console_height);
    drawer.size.min = k_console_min_height;
    drawer.resizable = true;
    drawer.slide = true;
    drawer.open = &chrome.console_open;
    if (oryx::gui::AreaScope console("console_drawer", drawer); console)
    {
        chrome.console->draw_console();
    }
    if (oryx::gui::AreaScope body("body"); body)
    {
        oryx::gui::PanelHostScope host;
        chrome.workbenches.draw();
        {
            oryx::gui::PanelScope showcase(k_showcase_panel);
            if (showcase.visible())
            {
                oryx::gui::show_showcase();
            }
        }
    }
    answer_switch(chrome.switching, chrome.dashboard->feed());
    oryx::gui::show_toasts();
}

void OasisLayer::menus()
{
    Chrome& chrome = *m_chrome;
    oryx::gui::begin_menu_bar("menu");
    game_menu(chrome.switching, chrome.board, chrome.dashboard->feed());
    opponent_menu(chrome.switching, chrome.board, chrome.dashboard->feed());
    view_menu();
    chrome.workbenches.window_menu();
    if (oryx::gui::begin_menu("Help"))
    {
        if (oryx::gui::menu_item("GUI Showcase", oryx::gui::is_panel_open(k_showcase_panel)).clicked)
        {
            static_cast<void>(oryx::gui::toggle_panel(k_showcase_panel));
        }
        oryx::gui::end_menu();
    }
    oryx::gui::end_menu_bar();
}

void OasisLayer::view_menu()
{
    Chrome& chrome = *m_chrome;
    if (!oryx::gui::begin_menu("View"))
    {
        return;
    }
    chrome.workbenches.menu();
    if (oryx::gui::menu_item("Synthetic search tree", chrome.observer->enabled()).clicked)
    {
        chrome.observer->set_enabled(!chrome.observer->enabled());
    }
    oryx::gui::end_menu();
}

#endif

bool OasisLayer::on_simulation_complete(oryx::SimulationCompleteEvent& event)
{
    if (event.benchmark())
    {
        OX_CORE_INFO("Benchmark complete: {}", oryx::format_benchmark_report(event.results()));
    }
    else
    {
        OX_CORE_INFO("Simulation complete: {} match(es), {} draw(s).", event.result().matches, event.result().draws);
    }
    return false;
}

} // namespace oasis
