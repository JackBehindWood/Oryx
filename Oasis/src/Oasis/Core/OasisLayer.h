#pragma once

#include "Options.h"

#include "Oryx/Events/SimulationEvent.h"

namespace oasis
{

// Everything specific to Oasis: decides how it starts (simulation, windowed or terminal play), reports simulation results and, windowed, is the app chrome: Game, Opponent, View, Window and Help menus, a status bar and the console drawer around
// a dock host of workbenches (the dashboard, the console, the GUI showcase), with the board's viewport panel's rect going to the input router. Placement, the layout file, focus and saved workbench state are the GUI's.
#ifdef OX_ENABLE_GRAPHICS
class OasisLayer : public oryx::Layer, public oryx::IFrameClient, public oryx::RenderSource
#else
class OasisLayer : public oryx::Layer
#endif
{
public:
    explicit OasisLayer(Options options);
    ~OasisLayer() noexcept override;

    void attach() override;
    void detach() override;
    void event(oryx::Event& event) override;

#ifdef OX_ENABLE_GRAPHICS
    // Builds the windowed chrome around `router`; start_graphics does this once the window and renderer exist, and a test calls it on a layer it never attaches.
    void use_graphics(oryx::InputRouter& router);
    // The board layer the menus switch; it is created after the chrome because it needs the feed.
    void bind(const oryx::BoardLayer& layer);
    // A match's board was created or is being released.
    void board_created(oryx::IGraphicsBoard& board, const std::string& game);
    void board_released(oryx::IGraphicsBoard& board);

    void frame(const oryx::FrameInfo& info) override;
    void render_stage(oryx::RenderStage stage, oryx::StageContext& context) override;

    // One frame of the chrome and its routing without the renderer, for a test with a font of its own.
    void run(const oryx::ImInput& input);
    void set_font(oryx::Font* font);

    [[nodiscard]] oryx::DashboardFeed& feed();
    // What the match reports to: the feed, through the optional synthetic tree.
    [[nodiscard]] oryx::IDecisionObserver& observer();
    [[nodiscard]] oryx::DashboardModel& model();
    [[nodiscard]] oryx::Workbenches& workbenches();
    [[nodiscard]] const oryx::GuiContext& context() const;
    [[nodiscard]] const oryx::gui::DockLayout& layout();
    [[nodiscard]] bool dashboard_open();
    [[nodiscard]] bool showcase_open();
    // Opens or closes the dashboard's panels, as Window > Dashboard does.
    void set_dashboard_open(bool open);
    [[nodiscard]] bool switch_pending() const;
    [[nodiscard]] bool console_open() const;
    [[nodiscard]] const oryx::AnnotationFlags& annotation_flags() const;
    [[nodiscard]] const oryx::BoardAnnotations& annotations() const;
#endif

private:
    void start_simulation();
    void start_play(const LaunchPlan& plan);
    // False leaves the application without a window or renderer so the caller can fall back to the terminal.
    bool start_graphics(const LaunchPlan& plan);
    bool on_simulation_complete(oryx::SimulationCompleteEvent& event);

    Options m_options;
#ifdef OX_ENABLE_GRAPHICS
    struct Chrome;
    void build();
    void menus();
    void view_menu();
    void claim_input();

    oryx::UniquePtr<Chrome> m_chrome;
#endif
};

} // namespace oasis
