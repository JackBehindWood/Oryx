#pragma once

#include "Oryx.h"

namespace oasis
{

// Long-lived graphics sandbox: draws a world scene and a top-left stats overlay through Renderer::begin_scene/draw_*/end_scene; GraphicsLayer owns the frame.
// Loads its font and texture through Assets; R reloads both from disk.
class PlaygroundLayer : public oryx::Layer
{
public:
    PlaygroundLayer();
    ~PlaygroundLayer() override;

    void attach() override;
    void detach() override;
    void update(double delta_time) override;
    void event(oryx::Event& event) override;

private:
    bool on_window_resize(oryx::WindowResizeEvent& event);
    bool on_window_focus(oryx::WindowFocusEvent& event);
    bool on_window_close(oryx::WindowCloseEvent& event);
    bool on_application_close(oryx::ApplicationCloseEvent& event);
    bool on_key_pressed(oryx::KeyPressedEvent& event);
    bool on_key_released(oryx::KeyReleasedEvent& event);
    bool on_mouse_moved(oryx::MouseMovedEvent& event);
    bool on_mouse_button_pressed(oryx::MouseButtonPressedEvent& event);
    bool on_mouse_button_released(oryx::MouseButtonReleasedEvent& event);
    bool on_mouse_scrolled(oryx::MouseScrolledEvent& event);

    void draw_world();
    void draw_overlay();
    void track_frame_time(double delta_time);

    oryx::Window* m_window = nullptr;
    oryx::AssetHandle<oryx::FontAsset> m_font_asset;
    oryx::AssetHandle<oryx::ImageAsset> m_image_asset;
    oryx::UniquePtr<oryx::Font> m_font;
    oryx::GpuAssetCache m_gpu_assets;
    oryx::Vec2f m_size;
    double m_time = 0.0;
    double m_fps_time = 0.0;
    uint32_t m_fps_frames = 0;
    float m_fps = 0.0f;
    float m_frame_ms = 0.0f;
    bool m_reload_down = false;
};

} // namespace oasis
