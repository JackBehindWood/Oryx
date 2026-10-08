#pragma once

#include "Oryx/Core/InputRouter.h"
#include "Oryx/Core/Layer.h"
#include "Oryx/Core/Settings.h"
#include "Oryx/Core/Timer.h"
#include "Oryx/Core/Window.h"
#include "Oryx/Graphics/RHI/RHIViewport.h"
#include "Oryx/Math/Colour.h"
#include "Oryx/Renderer/FrameClient.h"
#include "Oryx/Renderer/GraphicsSettings.h"

namespace oryx
{

class SceneRenderer;

// Interface clients run first and claim the pointer and keyboard; world clients then read the input that is left, in their own view.
enum class FramePhase : uint8_t
{
    World,
    Interface
};

struct ClientDesc
{
    FramePhase phase = FramePhase::World;
    // A world client's view; the main view is the whole surface until an interface client sets it.
    ViewId view = k_main_view;
};

// Pushed last so it runs after every game layer; owns the frame (viewport, clear colour, present) and the window pump.
class GraphicsLayer : public Layer
{
public:
    GraphicsLayer();

    void attach() override;
    void detach() override;
    void update(double delta_time) override;
    void event(Event& event) override;

    // Called once per frame, interface clients first, each phase in the order added, before the frame ends. Non-owning: the client must outlive the layer's detach or be removed first.
    void add_client(IFrameClient& client, const ClientDesc& desc = {});
    void remove_client(IFrameClient& client);
    [[nodiscard]] size_t client_count() const { return m_clients.size(); }

    // Interface clients claim input and declare views here during their frame.
    [[nodiscard]] InputRouter& router() { return m_router; }

    void set_clear_colour(const Colour& colour);
    [[nodiscard]] const Colour& clear_colour() const { return m_clear; }
    // Null when no Renderer existed at attach time.
    [[nodiscard]] const RHIViewportPtr& viewport() const { return m_viewport; }

private:
    void apply_settings(const GraphicsSettings& settings);
    struct Client
    {
        IFrameClient* client;
        ClientDesc desc;
    };

    void run_phases(const FrameInfo& info, SceneRenderer* scene);
    void run_clients(FramePhase phase, const FrameInfo& info, SceneRenderer* scene);

    Window* m_window = nullptr;
    // Created only when a Renderer exists at attach time; released in detach, before the device goes away.
    RHIViewportPtr m_viewport;
    std::vector<Client> m_clients;
    InputRouter m_router;
    Colour m_clear = { 0.08f, 0.08f, 0.1f, 1.0f };
    SettingsSubscription m_settings_subscription;
    bool m_applied_vsync = true;
    int32_t m_idle_sleep_ms = 50;
    FramePacer m_pacer;
    KeyCode m_reload_key = KeyCode::Unknown;
};

} // namespace oryx
