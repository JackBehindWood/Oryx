#pragma once

#include "Oryx/Core/Layer.h"
#include "Oryx/Core/Window.h"

namespace oryx
{

// Pushed last so it runs after every game layer; owns the per-frame window pump.
class GraphicsLayer : public Layer
{
public:
    GraphicsLayer();

    void attach() override;
    void update(double delta_time) override;
    void event(Event& event) override;

    [[nodiscard]] int32_t width() const { return m_width; }
    [[nodiscard]] int32_t height() const { return m_height; }

private:
    Window* m_window = nullptr;
    int32_t m_width = 0;
    int32_t m_height = 0;
};

} // namespace oryx
