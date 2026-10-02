#pragma once

#include "Oryx/Core/Layer.h"
#include "Oryx/Core/Window.h"
#include "Oryx/Graphics/RHI/RHIViewport.h"

namespace oryx
{

// Pushed last so it runs after every game layer; owns the per-frame window pump.
class GraphicsLayer : public Layer
{
public:
    GraphicsLayer();

    void attach() override;
    void detach() override;
    void update(double delta_time) override;
    void event(Event& event) override;

    [[nodiscard]] int32_t width() const { return m_width; }
    [[nodiscard]] int32_t height() const { return m_height; }

private:
    Window* m_window = nullptr;
    // Created only when a Renderer exists at attach time; released in detach, before the device goes away.
    RHIViewportPtr m_viewport;
    int32_t m_width = 0;
    int32_t m_height = 0;
};

} // namespace oryx
