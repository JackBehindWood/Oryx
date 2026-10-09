#pragma once

#include "Options.h"

#ifdef OX_ENABLE_GRAPHICS

#include "GuiShowcase.h"
#include "Oryx/Events/SimulationEvent.h"

namespace oasis
{

// The windowed Oasis chrome: Game, Opponent and Help menus over a dock host holding the board (viewport/0), the dashboard views and the GUI showcase as panels. The board's rect goes to the input router.
// An interface client; owns the dashboard feed, which the board layer feeds with every decision. Placement, the layout file and the View menu are the GUI's (gui::DockSession).
class OasisShell : public oryx::IFrameClient, public oryx::RenderSource
{
public:
    OasisShell(oryx::InputRouter& router, DashboardFlag flag);
    ~OasisShell() noexcept override;

    // The board layer the menus switch; it is created after the shell because it needs the feed.
    void bind(const oryx::BoardLayer& layer) { m_board = &layer; }

    void frame(const oryx::FrameInfo& info) override;
    void render_stage(oryx::RenderStage stage, oryx::StageContext& context) override;

    // One frame of the chrome and its routing without the renderer, for a test with a font of its own.
    void run(const oryx::ImInput& input);
    void set_font(oryx::Font* font);

    [[nodiscard]] oryx::DashboardFeed& feed() { return m_feed; }
    [[nodiscard]] const oryx::GuiContext& context() const { return m_context; }
    [[nodiscard]] const oryx::gui::DockLayout& layout() const { return m_dock->layout(); }
    // Any dashboard view panel is open.
    [[nodiscard]] bool dashboard_open() const;
    [[nodiscard]] bool showcase_open() const;
    [[nodiscard]] bool switch_pending() const { return m_switch_pending; }

private:
    void build();
    void menus();
    void game_menu();
    void opponent_menu();
    void request_switch(const std::string& game, const std::string& opponent);
    void answer_switch();
    void dispatch_switch();
    void view_menu();
    void apply_settings();
    void claim_input();
    void draw_showcase(const oryx::FrameInfo& info);

    oryx::InputRouter& m_router;
    const oryx::BoardLayer* m_board = nullptr;
    oryx::DashboardFeed m_feed;
    oryx::DashboardModel m_model;
    oryx::DashboardPanelState m_panel;
    oryx::GuiContext m_context;
    GuiShowcase m_showcase;
    oryx::SettingsSubscription m_settings_subscription;
    oryx::DashboardSettings m_settings;
    bool m_settings_dirty = false;
    oryx::UniquePtr<oryx::gui::DockSession> m_dock;
    oryx::gui::ForeignPanel m_showcase_panel;
    bool m_switch_pending = false;
    bool m_confirm_requested = false;
    std::string m_pending_game;
    std::string m_pending_opponent;
    oryx::Vec2f m_window{ 0.0f, 0.0f };
};

} // namespace oasis

#endif
