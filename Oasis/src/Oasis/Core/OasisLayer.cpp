#include "OasisLayer.h"

#include "SimulationRequest.h"

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
#else
constexpr bool k_graphics_built = false;
#endif

} // namespace

OasisLayer::OasisLayer(Options options)
    : oryx::Layer("OasisLayer")
    , m_options(std::move(options))
{
}

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
    m_showcase.reset();
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
    oryx::BoardFactory create_board = [&graphics](const std::string& game, oryx::PlayerId seat) -> oryx::SharedPtr<oryx::IBoard>
    {
        oryx::SharedPtr<oryx::IGraphicsBoard> board = oryx::create_graphics_board(game, seat);
        if (board != nullptr)
        {
            graphics.add_client(*board);
        }
        return board;
    };
    app.push_layer<oryx::BoardLayer>(oryx::BoardLayerDesc{ plan.game, m_options.opponent, create_board, false });
    m_showcase = oryx::create_unique<GuiShowcase>();
    graphics.add_client(*m_showcase);
    return true;
#else
    return false;
#endif
}

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
