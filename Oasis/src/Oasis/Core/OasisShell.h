#pragma once

#include "Options.h"

#ifdef OX_ENABLE_GRAPHICS

#include "GuiShowcase.h"
#include "Oryx/Events/SimulationEvent.h"

namespace oasis
{

// The windowed Oasis chrome: menu bar, the dashboard (or the GUI showcase) in a side panel, and the region left for the board, which it hands to the input router.
// An interface client; owns the dashboard feed, which the board layer feeds with every decision.
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
    [[nodiscard]] bool dashboard_open() const { return m_dashboard_open; }
    [[nodiscard]] bool showcase_open() const { return m_showcase_open; }
    [[nodiscard]] bool switch_pending() const { return m_switch_pending; }

private:
    void build();
    void menus();
    void game_menu();
    void opponent_menu();
    void request_switch(const std::string& game, const std::string& opponent);
    void answer_switch();
    void dispatch_switch();
    void apply_settings();
    void claim_input();

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
    bool m_dashboard_open;
    bool m_showcase_open = false;
    bool m_switch_pending = false;
    bool m_confirm_requested = false;
    std::string m_pending_game;
    std::string m_pending_opponent;
    oryx::Vec2f m_window{ 0.0f, 0.0f };
};

} // namespace oasis

#endif
