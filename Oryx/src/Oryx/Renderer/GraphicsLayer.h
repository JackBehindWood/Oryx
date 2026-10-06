#pragma once

#include "Oryx/Core/Layer.h"
#include "Oryx/Core/Window.h"
#include "Oryx/Graphics/RHI/RHIViewport.h"
#include "Oryx/Math/Colour.h"
#include "Oryx/Renderer/FrameClient.h"

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
    [[nodiscard]] int32_t width() const { return m_width; }
    [[nodiscard]] int32_t height() const { return m_height; }

private:
    Window* m_window = nullptr;
    // Created only when a Renderer exists at attach time; released in detach, before the device goes away.
    RHIViewportPtr m_viewport;
    std::vector<IFrameClient*> m_clients;
    Colour m_clear = { 0.08f, 0.08f, 0.1f, 1.0f };
    int32_t m_width = 0;
    int32_t m_height = 0;
};

} // namespace oryx
