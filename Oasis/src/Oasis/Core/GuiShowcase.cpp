#include "GuiShowcase.h"

#ifdef OX_ENABLE_GRAPHICS

namespace oasis
{

namespace
{

using namespace oryx;

constexpr float k_panel_width = 340.0f;
constexpr float k_panel_margin = 8.0f;
constexpr uint32_t k_image_size = 32;
constexpr float k_frame_budget_ms = 1000.0f / 60.0f;
const char* const k_tabs[] = { "Perf", "Widgets", "Data" };
constexpr uint32_t k_tab_count = 3;
constexpr uint32_t k_list_items = 500;
const char* const k_flush_reasons[FLUSH_REASON_COUNT] = { "explicit", "end", "stream change", "sampler change", "texture slots full", "index limit", "scissor change" };
const char* const k_command_kinds[6] = { "rect", "rounded", "border", "line", "text", "image" };
const std::string_view k_games[] = { "Tic-Tac-Toe", "Hexapawn", "Connect Four" };

float lap_ms(Timer& timer)
{
    return static_cast<float>(timer.tick() * 1000.0);
}

void number_row(std::string_view key, uint64_t value)
{
    if (value != 0)
    {
        gui::key_value(key, gui::context().arena().format("%llu", static_cast<unsigned long long>(value)));
    }
}

void bytes_row(std::string_view key, uint64_t bytes)
{
    if (bytes != 0)
    {
        gui::key_value(key, gui::context().arena().format("%.1f KiB", static_cast<double>(bytes) / 1024.0));
    }
}

void ms_row(std::string_view key, float milliseconds)
{
    if (milliseconds > math::EPSILON<float>)
    {
        gui::key_value(key, gui::context().arena().format("%.3f ms", static_cast<double>(milliseconds)));
    }
}

// One bar split into `count` coloured segments sized by `values`; zero segments take no room.
void stacked_bar(std::string_view name, const float* values, uint32_t count)
{
    float total = 0.0f;
    for (uint32_t index = 0; index < count; ++index)
    {
        total += values[index];
    }
    im::CanvasArea area = gui::canvas(name, grow(), fixed(14.0f));
    area.painter.fill_rect(area.rect, { 0.0f, 0.0f, 0.0f, 0.25f });
    if (total <= math::EPSILON<float>)
    {
        return;
    }
    float x = area.rect.min[0];
    for (uint32_t index = 0; index < count; ++index)
    {
        const float width = area.rect.size[0] * values[index] / total;
        area.painter.fill_rect({ { x, area.rect.min[1] }, { width, area.rect.size[1] } }, gui::theme().palette[index % 8]);
        x += width;
    }
}

float percentile(const float* sorted, uint32_t count, float fraction)
{
    return sorted[static_cast<uint32_t>(fraction * static_cast<float>(count - 1) + 0.5f)];
}

} // namespace

GuiShowcase::GuiShowcase()
{
    for (float& sample : m_frame_ms)
    {
        sample = k_frame_budget_ms;
    }
    m_rows.resize(k_table_rows);
    for (uint32_t index = 0; index < k_table_rows; ++index)
    {
        const uint32_t hash = index * 2654435761u;
        m_rows[index] = { index, static_cast<float>(hash % 2001u) / 1000.0f - 1.0f, hash % 9973u };
    }
}

GuiShowcase::~GuiShowcase() noexcept = default;

void GuiShowcase::sort_rows(uint32_t column, bool ascending)
{
    const auto less = [column](const TableRow& a, const TableRow& b)
    {
        return column == 0 ? a.ply < b.ply : column == 1 ? a.value < b.value : a.visits < b.visits;
    };
    std::sort(m_rows.begin(), m_rows.end(), [&](const TableRow& a, const TableRow& b) { return ascending ? less(a, b) : less(b, a); });
}

void GuiShowcase::set_font(Font* font)
{
    GuiTheme theme = m_context.gui_theme();
    theme.font = font;
    theme.min_hit_size = 18.0f;
    m_context.set_theme(theme);
}

void GuiShowcase::ensure_image()
{
    if (m_image != nullptr)
    {
        return;
    }
    std::vector<uint8_t> pixels(k_image_size * k_image_size * 4);
    for (uint32_t y = 0; y < k_image_size; ++y)
    {
        for (uint32_t x = 0; x < k_image_size; ++x)
        {
            const bool dark = ((x / 8) + (y / 8)) % 2 == 0;
            uint8_t* pixel = &pixels[(y * k_image_size + x) * 4];
            pixel[0] = static_cast<uint8_t>(x * 255 / (k_image_size - 1));
            pixel[1] = static_cast<uint8_t>(y * 255 / (k_image_size - 1));
            pixel[2] = dark ? 90 : 220;
            pixel[3] = 255;
        }
    }
    Texture2DDesc desc;
    desc.width = k_image_size;
    desc.height = k_image_size;
    desc.pixels = pixels.data();
    desc.pixel_bytes = static_cast<uint32_t>(pixels.size());
    m_image = create_unique<Texture2D>(Texture2D::create(Renderer::rhi(), desc));
}

void GuiShowcase::frame(const FrameInfo& info)
{
    if (m_context.gui_theme().font == nullptr)
    {
        set_font(&Renderer::default_font());
    }
    ensure_image();
    m_window = info.logical;
    const Numbers numbers{ Renderer::last_frame_stats(), Renderer::pipeline_cache_stats() };
    run(make_im_input(info.input, 0, info.logical, info.scale, static_cast<float>(info.delta_time)), numbers);
    Renderer::scene().submit(*this);
}

void GuiShowcase::render_stage(RenderStage stage, StageContext& context)
{
    if (stage != RenderStage::Overlay)
    {
        return;
    }
    Timer timer;
    timer.start();
    const Texture2D* const images[1] = { m_image.get() };
    ReplayTarget target{ { 0.0f, 0.0f }, m_window };
    target.images = images;
    target.image_count = 1;
    const BatchStats before = context.batcher_2d.stats();
    replay(m_context.draw_list(), context.batcher_2d, *m_context.gui_theme().font, target);
    const BatchStats& after = context.batcher_2d.stats();
    m_replay_ms = lap_ms(timer);
    m_replay_batch = batch_stats_delta(after, before);
}

void GuiShowcase::update_series(float delta_time)
{
    const float ms = delta_time * 1000.0f;
    m_frame_ms[m_frame_head] = ms;
    m_frame_head = (m_frame_head + 1) % k_history;
    if (delta_time > math::EPSILON<float>)
    {
        m_fps = math::lerp(m_fps, 1.0f / delta_time, 0.1f);
    }
    m_time += delta_time;
    for (uint32_t index = 0; index < 96; ++index)
    {
        const float phase = static_cast<float>(index) * 0.15f + m_time * 1.5f;
        m_wave[index] = math::sin(phase) + 0.4f * math::sin(phase * 2.7f);
    }
    for (uint32_t index = 0; index < 9; ++index)
    {
        m_policy[index] = 0.5f + 0.5f * math::sin(m_time * 0.8f + static_cast<float>(index));
    }
}

void GuiShowcase::run(const ImInput& input, const Numbers& numbers)
{
    ContextScope<GuiContext> scope(m_context);
    update_series(input.delta_time);
    try
    {
        Timer timer;
        timer.start();
        m_context.begin_frame(input);
        build(numbers);
        m_build_ms = lap_ms(timer);
        m_context.end_frame();
        m_solve_ms = lap_ms(timer);
    }
    catch (...)
    {
        m_context.abort_frame();
        throw;
    }
    m_gui_stats = m_context.stats();
}

void GuiShowcase::build(const Numbers& numbers)
{
    const Vec2f surface = m_context.input().surface_size;
    LayoutStyle layout = gui::anchored(AttachPoint::TopRight, fixed(k_panel_width), fixed(math::max(surface[1] - 2.0f * k_panel_margin, 0.0f)), { -k_panel_margin, k_panel_margin });
    layout.direction = Direction::Column;
    layout.padding = uniform_insets(8.0f);
    layout.gap = gui::spacing();
    gui::WidgetOptions options;
    options.layout = &layout;
    {
        gui::PanelScope panel("showcase", options);
        gui::label("GUI showcase (temporary)");
        {
            gui::TabBarScope tabs("tabs", m_tab);
            for (uint32_t index = 0; index < k_tab_count; ++index)
            {
                std::ignore = tabs.tab(k_tabs[index], &m_show_tab[index]);
            }
        }
        for (uint32_t index = 0; index < k_tab_count; ++index)
        {
            if (!m_show_tab[index] && m_tab == index)
            {
                m_tab = k_tab_count;
                for (uint32_t other = 0; other < k_tab_count && m_tab == k_tab_count; ++other)
                {
                    m_tab = m_show_tab[other] ? other : k_tab_count;
                }
            }
        }
        {
            gui::RowScope closed("closed tabs", { .gap = 4.0f });
            for (uint32_t index = 0; index < k_tab_count; ++index)
            {
                if (!m_show_tab[index] && gui::small_button(k_tabs[index]).clicked)
                {
                    m_show_tab[index] = true;
                    m_tab = index;
                }
            }
        }
        gui::separator();
        if (m_tab >= k_tab_count)
        {
            gui::label("All tabs are closed");
            gui::show_toasts();
            return;
        }
        gui::ScrollScope body(k_tabs[m_tab]);
        if (m_tab == 0)
        {
            perf(numbers);
        }
        else if (m_tab == 1)
        {
            widgets();
        }
        else
        {
            data();
        }
    }
    gui::show_toasts();
}

void GuiShowcase::replay_view()
{
    struct Stage
    {
        const char* title;
        const char* caption;
        std::string_view numbers;
    };
    FrameArena& arena = m_context.arena();
    const Stage stages[3] = {
        { "1  Build", "your widget calls become layout boxes",
          arena.format("%u boxes, %u interactive items   %.2f ms", m_gui_stats.boxes, m_gui_stats.items, static_cast<double>(m_build_ms)) },
        { "2  Solve and paint", "the layout is sized and drawn into a list of plain shapes",
          arena.format("%u shapes (rects, lines, text...)   %.2f ms", m_gui_stats.commands, static_cast<double>(m_solve_ms)) },
        { "3  Replay", "the list becomes GPU quads and glyphs, clips become scissors",
          arena.format("%u prims, %u vertices, %.1f KiB, %u scissors   %.2f ms", m_replay_batch.primitives, m_replay_batch.vertices, static_cast<double>(m_replay_batch.bytes) / 1024.0,
                      m_replay_batch.flushes[static_cast<uint32_t>(FlushReason::ScissorChange)], static_cast<double>(m_replay_ms))
        }
    };
    constexpr float k_box_height = 50.0f;
    constexpr float k_arrow = 12.0f;
    const float text_height = gui::theme().base.text_height * 0.75f;
    const ImStyle& style = gui::theme().base;
    im::CanvasArea area = gui::canvas("replay pipeline", grow(), fixed(3.0f * k_box_height + 2.0f * k_arrow));
    for (uint32_t index = 0; index < 3; ++index)
    {
        const float top = area.rect.min[1] + static_cast<float>(index) * (k_box_height + k_arrow);
        const Rect box = { { area.rect.min[0], top }, { area.rect.size[0], k_box_height } };
        Colour tint = gui::theme().palette[index];
        tint.a = 0.25f;
        area.painter.fill_rounded_rect(box, uniform_radius(style.radius), tint);
        area.painter.fill_rect({ box.min, { 3.0f, box.size[1] } }, gui::theme().palette[index]);
        const float x = box.min[0] + 8.0f;
        const float width = box.size[0] - 12.0f;
        area.painter.text({ { x, top + 2.0f }, { width, text_height + 2.0f } }, stages[index].title, { text_height, style.text, TextAlign::Left, true });
        Colour muted = style.text;
        muted.a = 0.7f;
        area.painter.text({ { x, top + 2.0f + text_height + 2.0f }, { width, text_height + 2.0f } }, stages[index].caption, { text_height, muted, TextAlign::Left, true });
        area.painter.text({ { x, top + 2.0f + 2.0f * (text_height + 2.0f) }, { width, text_height + 2.0f } }, stages[index].numbers, { text_height, style.text, TextAlign::Left, true });
        if (index < 2)
        {
            const float centre = box.min[0] + box.size[0] * 0.5f;
            const float from = top + k_box_height + 1.0f;
            const float to = from + k_arrow - 2.0f;
            area.painter.line({ centre, from }, { centre, to }, style.text, 1.5f);
            area.painter.line({ centre - 4.0f, to - 4.0f }, { centre, to }, style.text, 1.5f);
            area.painter.line({ centre + 4.0f, to - 4.0f }, { centre, to }, style.text, 1.5f);
        }
    }
    gui::label("Time per stage");
    const float times[3] = { m_build_ms, m_solve_ms, m_replay_ms };
    stacked_bar("stages", times, 3);
    {
        gui::RowScope row("stage legend", { .gap = 8.0f });
        gui::legend_item("build", gui::theme().palette[0]);
        gui::legend_item("solve and paint", gui::theme().palette[1]);
        gui::legend_item("replay", gui::theme().palette[2]);
    }
    gui::label("Shapes in the list, by kind");
    const float kinds[6] = { static_cast<float>(m_gui_stats.rects), static_cast<float>(m_gui_stats.rounded_rects), static_cast<float>(m_gui_stats.borders),
                             static_cast<float>(m_gui_stats.lines), static_cast<float>(m_gui_stats.texts), static_cast<float>(m_gui_stats.images) };
    stacked_bar("kinds", kinds, 6);
    {
        gui::RowScope row("kind legend", { .gap = 8.0f });
        for (uint32_t index = 0; index < 6; ++index)
        {
            if (kinds[index] > 0.0f)
            {
                gui::legend_item(k_command_kinds[index], gui::theme().palette[index]);
            }
        }
    }
}

void GuiShowcase::replay_streams()
{
    FrameArena& arena = m_context.arena();
    gui::label("Replay by stream");
    for (uint32_t stream = 0; stream < PRIMITIVE_2D_COUNT; ++stream)
    {
        const BatchStreamStats& stats = m_replay_batch.streams[stream];
        if (stats.primitives != 0)
        {
            gui::key_value(arena.format("stream %s", primitive_name(static_cast<Primitive2D>(stream))),
                           arena.format("%u prims, %u verts, %.1f KiB, %u draws", stats.primitives, stats.vertices, static_cast<double>(stats.bytes) / 1024.0, stats.draws));
        }
    }
    gui::label("Replay flushes");
    for (uint32_t reason = 0; reason < FLUSH_REASON_COUNT; ++reason)
    {
        number_row(arena.format("replay %s", k_flush_reasons[reason]), m_replay_batch.flushes[reason]);
    }
    gui::label("Replay stream switches");
    for (uint32_t from = 0; from < PRIMITIVE_2D_COUNT; ++from)
    {
        for (uint32_t to = 0; to < PRIMITIVE_2D_COUNT; ++to)
        {
            number_row(arena.format("%s > %s", primitive_name(static_cast<Primitive2D>(from)), primitive_name(static_cast<Primitive2D>(to))), m_replay_batch.stream_switches[from][to]);
        }
    }
}

void GuiShowcase::perf(const Numbers& numbers)
{
    {
        gui::RowScope row("panels", { .gap = 6.0f });
        std::ignore = gui::checkbox("frame", m_show_frame);
        std::ignore = gui::checkbox("renderer", m_show_renderer);
        std::ignore = gui::checkbox("flushes", m_show_flushes);
        std::ignore = gui::checkbox("gui", m_show_gui);
        std::ignore = gui::checkbox("replay", m_show_replay);
        std::ignore = gui::checkbox("inspector", m_show_inspector);
    }
    const FrameStats& frame = numbers.frame;
    const BatchStats& batch = frame.batch;
    if (gui::collapsing_header("frame time", &m_show_frame, true))
    {
        const ColourScale fps_scale = sequential_scale(0.0f, 120.0f);
        gui::BarOptions fps_options;
        fps_options.format = { NumberStyle::Fixed, 0 };
        fps_options.scale = &fps_scale;
        gui::bar("fps", m_fps, 120.0f, fps_options);
        {
            gui::PlotOptions options;
            options.height = fixed(70.0f);
            options.y_min = 0.0f;
            gui::PlotScope plot("frame ms", options);
            plot.line("ms", values_ring(m_frame_ms, k_history, m_frame_head));
            plot.hline(k_frame_budget_ms, { 0.9f, 0.3f, 0.3f, 1.0f });
        }
        float sorted[k_history];
        std::copy(m_frame_ms, m_frame_ms + k_history, sorted);
        std::sort(sorted, sorted + k_history);
        ms_row("p50", percentile(sorted, k_history, 0.5f));
        ms_row("p95", percentile(sorted, k_history, 0.95f));
        ms_row("p99", percentile(sorted, k_history, 0.99f));
        ms_row("max", sorted[k_history - 1]);
        ms_row("clients", frame.cpu.clients_ms);
        ms_row("scene end", frame.cpu.scene_ms);
        ms_row("record and present", frame.cpu.record_ms);
        ms_row("gui build", m_build_ms);
        ms_row("gui solve and paint", m_solve_ms);
        ms_row("gui replay", m_replay_ms);
    }
    if (gui::collapsing_header("renderer", &m_show_renderer, true))
    {
        number_row("passes", frame.passes);
        number_row("draw items", frame.draw_items);
        number_row("draws", batch.draws);
        number_row("primitives", batch.primitives);
        number_row("vertices", batch.vertices);
        number_row("triangles", batch.triangles);
        number_row("texture slots used", batch.texture_slots_used);
        bytes_row("vertex data this frame", batch.bytes);
        number_row("batch pages", batch.pages);
        number_row("pipelines", numbers.pipelines.entries);
        number_row("pipeline hits", frame.pipeline_hits);
        number_row("pipeline misses", frame.pipeline_misses);
        number_row("allocations", frame.allocations);
        bytes_row("heap in use (whole process)", static_cast<uint64_t>(math::max<int64_t>(frame.live_bytes, 0)));
    }
    if (gui::collapsing_header("flushes", &m_show_flushes))
    {
        for (uint32_t reason = 0; reason < FLUSH_REASON_COUNT; ++reason)
        {
            number_row(k_flush_reasons[reason], batch.flushes[reason]);
        }
    }
    if (gui::collapsing_header("gui", &m_show_gui, true))
    {
        number_row("interactive items", m_gui_stats.items);
        number_row("layout boxes", m_gui_stats.boxes);
        number_row("commands", m_gui_stats.commands);
        number_row("remembered items", m_gui_stats.remembered_items);
        number_row("state entries", m_gui_stats.state_entries);
        bytes_row("arena used", m_gui_stats.arena_used);
        bytes_row("arena high water", m_gui_stats.arena_high_water);
        bytes_row("arena capacity", m_gui_stats.arena_capacity);
    }
    if (gui::collapsing_header("replay", &m_show_replay, true))
    {
        replay_view();
        replay_streams();
    }
    if (gui::collapsing_header("inspector", &m_show_inspector))
    {
        gui::inspector();
    }
}

void GuiShowcase::widgets()
{
    if (gui::collapsing_header("basics", true))
    {
        gui::label("A label");
        gui::bullet("A bullet");
        const ItemState button = gui::button("Toast");
        if (button.clicked)
        {
            gui::toast("Hello from the GUI", 2.0f);
        }
        gui::tooltip(button, "Shows a message for two seconds");
        std::ignore = gui::checkbox("checkbox", m_flag);
        std::ignore = gui::radio("first", m_radio, 0);
        std::ignore = gui::radio("second", m_radio, 1);
        gui::badge("badge", { 0.0f, 0.62f, 0.45f, 1.0f });
        gui::progress("progress", m_slider, {});
    }
    if (gui::collapsing_header("input", true))
    {
        std::ignore = gui::slider_float("slider", m_slider, 0.0f, 1.0f);
        std::ignore = gui::slider_int("count", m_count, 0, 10);
        std::ignore = gui::drag_float("drag", m_drag, 0.0f, 100.0f);
        {
            gui::ComboScope combo("game", k_games[m_combo]);
            for (int32_t index = 0; index < 3; ++index)
            {
                if (combo.item(k_games[index], index == m_combo))
                {
                    m_combo = index;
                }
            }
        }
        gui::ListBoxScope list("list, 500 items", { .item_count = k_list_items });
        for (uint32_t index = list.first_item(); index < list.last_item(); ++index)
        {
            if (list.item(index, m_context.arena().format("item %u", index), static_cast<int32_t>(index) == m_list).clicked)
            {
                m_list = static_cast<int32_t>(index);
            }
        }
    }
    if (gui::collapsing_header("tree"))
    {
        gui::TreeScope root("root", { .default_open = true });
        if (root.open())
        {
            gui::label("leaf one");
            gui::TreeScope branch("branch");
            if (branch.open())
            {
                gui::label("leaf two");
            }
        }
    }
    if (gui::collapsing_header("images", true))
    {
        gui::RowScope row("images", { .gap = 6.0f });
        std::ignore = gui::image("plain", k_single_image, { .width = fixed(40.0f), .height = fixed(40.0f) });
        std::ignore = gui::image("rounded", k_single_image, { .width = fixed(40.0f), .height = fixed(40.0f), .radius = uniform_radius(10.0f) });
        std::ignore = gui::image("circle", k_single_image, { .width = fixed(40.0f), .height = fixed(40.0f), .radius = im::circle_radius() });
        gui::ImageButtonOptions button_options;
        button_options.width = fixed(32.0f);
        button_options.height = fixed(32.0f);
        if (gui::image_button("button", k_single_image, button_options).clicked)
        {
            gui::toast("Image button clicked", 2.0f);
        }
    }
}

void GuiShowcase::data()
{
    if (gui::collapsing_header("policy heat grid", true))
    {
        gui::HeatGridOptions options;
        options.cell = { 40.0f, 28.0f };
        options.show_values = true;
        options.format = { NumberStyle::Fixed, 2 };
        const gui::HeatGridResult result = gui::heat_grid("policy", values(m_policy), 3, sequential_scale(), options);
        m_cell = result.clicked_index != gui::k_no_index ? result.clicked_index : m_cell;
        gui::key_value("selected cell", m_cell == gui::k_no_index ? std::string_view("none") : m_context.arena().format("%u", m_cell));
    }
    if (gui::collapsing_header("plots", true))
    {
        {
            gui::PlotOptions options;
            options.height = fixed(90.0f);
            gui::PlotScope plot("waves", options);
            plot.line("line", values(m_wave));
            plot.line("step", values_ring(m_wave, 96, 20), gui::PlotStyle::Step);
            plot.hline(0.0f, { 0.6f, 0.6f, 0.6f, 1.0f });
            if (plot.result().hover_index != gui::k_no_index)
            {
                plot.vline(static_cast<float>(plot.result().hover_index), { 1.0f, 1.0f, 1.0f, 0.6f });
            }
        }
        std::ignore = gui::plot_histogram("distribution", values(m_policy), { .height = fixed(60.0f), .y_min = 0.0f, .y_max = 1.0f });
        gui::RowScope row("sparklines", { .gap = 8.0f });
        std::ignore = gui::sparkline("wave", values(m_wave));
        std::ignore = gui::sparkline("area", values(m_policy), { .colour = { 0.9f, 0.62f, 0.0f, 1.0f } });
        gui::legend_item("wave", m_context.gui_theme().palette[0]);
        gui::legend_item("policy", { 0.9f, 0.62f, 0.0f, 1.0f });
    }
    if (gui::collapsing_header("table, 1000 rows", true))
    {
        gui::TableOptions options;
        options.row_count = k_table_rows;
        options.height = fixed(220.0f);
        gui::TableScope table("trace", options);
        table.column("Ply", fixed(60.0f), true);
        table.column("Value", grow(), true, TextAlign::Right);
        table.column("Visits", fixed(80.0f), true, TextAlign::Right);
        const gui::TableResult head = table.headers();
        if (head.sort_changed)
        {
            sort_rows(head.sort_column, head.sort_ascending);
        }
        for (uint32_t index = table.first_row(); index < table.last_row(); ++index)
        {
            const TableRow& row = m_rows[index];
            if (table.row(index, static_cast<int32_t>(index) == m_row).clicked)
            {
                m_row = static_cast<int32_t>(index);
            }
            FrameArena& arena = m_context.arena();
            table.cell(arena.format("%u", row.ply));
            table.cell(arena.format("%.3f", static_cast<double>(row.value)));
            table.cell(arena.format("%u", row.visits));
        }
    }
}

} // namespace oasis

#endif
