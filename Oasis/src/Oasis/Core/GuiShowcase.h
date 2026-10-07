#pragma once

#include "Oryx.h"

#ifdef OX_ENABLE_GRAPHICS

namespace oasis
{

// Temporary until the Dashboard panel of Phase 11 Step 6: a live tour of the GUI widgets over the Oasis window, with the renderer's own numbers.
class GuiShowcase : public oryx::IFrameClient, public oryx::RenderSource
{
public:
    struct Numbers
    {
        oryx::FrameStats frame;
        oryx::GraphicsPipelineCacheStats pipelines;
    };

    GuiShowcase();
    ~GuiShowcase() noexcept override;

    void frame(const oryx::FrameInfo& info) override;
    void render_stage(oryx::RenderStage stage, oryx::StageContext& context) override;

    // One frame of the panel without the renderer, for a test with a font of its own.
    void run(const oryx::ImInput& input, const Numbers& numbers);
    void set_font(oryx::Font* font);
    [[nodiscard]] const oryx::GuiContext& context() const { return m_context; }

private:
    static constexpr uint32_t k_history = 240;
    static constexpr uint32_t k_table_rows = 1000;

    struct TableRow
    {
        uint32_t ply = 0;
        float value = 0.0f;
        uint32_t visits = 0;
    };

    void build(const Numbers& numbers);
    void perf(const Numbers& numbers);
    void replay_view();
    void replay_streams();
    void widgets();
    void data();
    void update_series(float delta_time);
    void ensure_image();
    void sort_rows(uint32_t column, bool ascending);

    oryx::GuiContext m_context;
    oryx::ImStats m_gui_stats;
    float m_build_ms = 0.0f;
    float m_solve_ms = 0.0f;
    float m_replay_ms = 0.0f;
    // What replaying the GUI's draw list added to the frame's batcher.
    oryx::BatchStats m_replay_batch;
    oryx::UniquePtr<oryx::Texture2D> m_image;
    oryx::Vec2f m_window{ 0.0f, 0.0f };
    float m_frame_ms[k_history] = {};
    uint32_t m_frame_head = 0;
    float m_fps = 60.0f;
    float m_time = 0.0f;
    float m_wave[96] = {};
    float m_policy[9] = {};
    uint32_t m_tab = 0;
    bool m_show_tab[3] = { true, true, true };
    bool m_show_frame = true;
    bool m_show_renderer = true;
    bool m_show_flushes = false;
    bool m_show_gui = true;
    bool m_show_replay = true;
    bool m_show_inspector = false;

    bool m_flag = true;
    int32_t m_radio = 0;
    float m_slider = 0.5f;
    int32_t m_count = 3;
    float m_drag = 1.0f;
    int32_t m_combo = 0;
    int32_t m_list = 1;
    uint32_t m_cell = oryx::gui::k_no_index;
    std::vector<TableRow> m_rows;
    int32_t m_row = -1;
};

} // namespace oasis

#endif
