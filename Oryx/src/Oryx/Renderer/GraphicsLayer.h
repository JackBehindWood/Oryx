#pragma once

#include "Oryx/Core/Layer.h"
#include "Oryx/Core/Settings.h"
#include "Oryx/Core/Window.h"
#include "Oryx/Graphics/RHI/RHIViewport.h"
#include "Oryx/Math/Colour.h"
#include "Oryx/Renderer/FrameClient.h"
#include "Oryx/Renderer/GraphicsSettings.h"

namespace oryx
{

// Pushed last so it runs after every game layer; owns the frame (viewport, clear colour, present) and the window pump.
class GraphicsLayer : public Layer
{
public:
    GraphicsLayer();

    void attach() override;
    void detach() override;
    void update(double delta_time) override;
    void event(Event& event) override;

    // Called once per frame, in the order added, before the frame ends. Non-owning: the client must outlive the layer's detach or be removed first.
    void add_client(IFrameClient& client);
    void remove_client(IFrameClient& client);

    void set_clear_colour(const Colour& colour);
    [[nodiscard]] const Colour& clear_colour() const { return m_clear; }
    // Null when no Renderer existed at attach time.
    [[nodiscard]] const RHIViewportPtr& viewport() const { return m_viewport; }

private:
    void apply_settings(const GraphicsSettings& settings);
    void pace_frame();

    Window* m_window = nullptr;
    // Created only when a Renderer exists at attach time; released in detach, before the device goes away.
    RHIViewportPtr m_viewport;
    std::vector<IFrameClient*> m_clients;
    Colour m_clear = { 0.08f, 0.08f, 0.1f, 1.0f };
    SettingsSubscription m_settings_subscription;
    bool m_applied_vsync = true;
    std::chrono::milliseconds m_idle_sleep{ 50 };
    std::chrono::nanoseconds m_frame_period{ 0 };
    KeyCode m_reload_key = KeyCode::Unknown;
    std::chrono::steady_clock::time_point m_next_frame;
};

} // namespace oryx
