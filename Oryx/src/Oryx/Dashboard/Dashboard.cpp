#include "oxpch.h"
#include "Oryx/Dashboard/Dashboard.h"

#include "Oryx/Board/Graphics/DecisionAnnotations.h"
#include "Oryx/Board/Graphics/PresenterActionDescriber.h"

namespace oryx
{

namespace
{

constexpr float k_view_min_width = 160.0f;
constexpr float k_view_min_height = 96.0f;
constexpr float k_board_min_width = 200.0f;
constexpr float k_board_min_height = 120.0f;

std::string slug_of(std::string_view title)
{
    std::string slug;
    for (const char c : title)
    {
        slug += std::isalnum(static_cast<unsigned char>(c)) ? static_cast<char>(std::tolower(static_cast<unsigned char>(c))) : '-';
    }
    return slug;
}

AnnotationFlags flags_of(const DashboardSettings& settings)
{
    AnnotationFlags flags;
    for (const std::string& name : settings.annotations)
    {
        flags.heat = flags.heat || name == "heat";
        flags.chosen = flags.chosen || name == "chosen";
        flags.value = flags.value || name == "value";
    }
    return flags;
}

FeedOptions feed_options(const DashboardSettings& settings)
{
    FeedOptions options;
    options.capacity = settings.history;
    options.max_tree_nodes = settings.max_tree_nodes;
    return options;
}

} // namespace

Dashboard::Dashboard()
    : Workbench("dashboard", WorkbenchOptions{ .always_update = true })
    , m_feed(feed_options(settings_of<DashboardSettings>()))
    , m_settings(settings_of<DashboardSettings>())
{
    m_model.feed = &m_feed;
    m_flags = flags_of(m_settings);
    m_pushed_flags = m_flags;
    m_saved = DashboardState{ m_flags.heat, m_flags.chosen, m_flags.value };
    persist(m_saved);
    m_subscription = on_settings_changed<DashboardSettings>(
        [this](const DashboardSettings& changed)
        {
            m_settings = changed;
            m_settings_dirty = true;
        });
}

Dashboard::~Dashboard() noexcept = default;

void Dashboard::declare_panels()
{
    gui::PanelOptions board;
    board.title = "Board";
    board.kind = gui::PanelKind::Viewport;
    board.min_w = k_board_min_width;
    board.min_h = k_board_min_height;
    board.toolbar = true;
    if (!gui::register_panel(k_viewport_panel, board))
    {
        throw Error("the board's viewport panel could not be registered");
    }
}

void Dashboard::add_view(const std::string& id)
{
    UniquePtr<IDashboardView> view = DashboardViewRegistry::create(id);
    if (view == nullptr)
    {
        OX_WARN("Dashboard view '{}' is not registered", id);
        return;
    }
    add_view(std::move(view));
}

void Dashboard::add_view(UniquePtr<IDashboardView> view)
{
    const DashboardViewPlacement placement = view->placement();
    const size_t side = static_cast<size_t>(placement.side);
    gui::PanelOptions options;
    options.min_w = k_view_min_width;
    options.min_h = k_view_min_height;
    options.initial_open = true;
    options.dock_near = placement.beside_board ? std::string_view(k_viewport_panel) : std::string_view();
    options.dock_side = placement.side;
    options.dock_size = placement.size;
    options.order = static_cast<int32_t>(m_views.size()) + 1;
    if (!m_side_anchor[side].empty())
    {
        options.dock_tabbed_with = m_side_anchor[side];
    }
    const PanelHandle handle = add_panel(slug_of(view->title()), view->title(), options);
    if (m_side_anchor[side].empty())
    {
        m_side_anchor[side] = std::string(panel_name(handle));
    }
    m_views.push_back(std::move(view));
}

void Dashboard::draw()
{
    Workbench::draw();
    draw_board_toolbar();
}

void Dashboard::draw_board_toolbar()
{
    if (gui::PanelToolbarScope bar(k_viewport_panel); bar.visible())
    {
        gui::caption("Overlays");
        gui::toggle("Heat", m_flags.heat);
        gui::toggle("Chosen", m_flags.chosen);
        gui::toggle("Values", m_flags.value);
    }
}

void Dashboard::draw_panel(PanelHandle panel, const PanelFrame& frame)
{
    sanitise_selection(m_model);
    m_model.keyboard_focus = frame.keyboard_ok;
    m_views[panel.index]->draw(m_model);
}

void Dashboard::state_loaded()
{
    m_flags = AnnotationFlags{ m_saved.heat, m_saved.chosen, m_saved.value };
}

void Dashboard::apply_settings()
{
    if (m_settings_dirty)
    {
        m_settings_dirty = false;
        m_flags = flags_of(m_settings);
    }
}

void Dashboard::tick(const WorkbenchFrame&)
{
    m_model.hovered_action = INVALID_ACTION;
    apply_settings();
    sync_board();
    m_saved = DashboardState{ m_flags.heat, m_flags.chosen, m_flags.value };
}

void Dashboard::board_created(IGraphicsBoard& board, const std::string& game)
{
    m_graphics_board = &board;
    m_pushed_flags = board.annotation_flags();
    m_feed.set_describer(make_presenter_describer(game));
    m_model.pinned_action = INVALID_ACTION;
    m_model.board_hovered_action = INVALID_ACTION;
    m_annotations.items.clear();
}

void Dashboard::board_released(IGraphicsBoard& board)
{
    if (m_graphics_board == &board)
    {
        m_graphics_board = nullptr;
    }
}

void Dashboard::sync_board()
{
    if (m_graphics_board == nullptr)
    {
        return;
    }
    const AnnotationFlags on_board = m_graphics_board->annotation_flags();
    if (!(on_board == m_pushed_flags))
    {
        m_flags = on_board;
    }
    const bool enabled = open();
    if (m_feed.match_index() != m_seen_match)
    {
        m_seen_match = m_feed.match_index();
        m_model.pinned_action = INVALID_ACTION;
        follow_latest_record(m_model);
    }
    sanitise_selection(m_model);
    RecordView selected = selected_view(m_model);
    // A restart leaves the last decision of the finished match as the newest record; nothing of it belongs on the fresh board.
    if (selected.valid && selected.record->match_index != m_feed.match_index())
    {
        selected = RecordView{};
    }
    annotate_decision(selected, m_annotations);
    const bool ghost = enabled && selected.valid && m_feed.size() > 0 && selected.record->sequence != m_feed.view(m_feed.size() - 1).record->sequence;
    m_legend.clear();
    if (enabled && selected.valid)
    {
        char text[96];
        std::snprintf(text, sizeof(text), "Ply %u \xC2\xB7 Player %d \xC2\xB7 %s", selected.record->decision_index + 1, static_cast<int32_t>(selected.record->player) + 1, m_board != nullptr ? m_board->opponent_name().c_str() : "");
        m_legend = text;
    }
    m_model.board_hovered_action = enabled ? action_ending_at(selected, m_graphics_board->hovered_space()) : INVALID_ACTION;
    BoardAnnotationRequest request;
    request.annotations = enabled ? &m_annotations : nullptr;
    request.flags = enabled ? m_flags : AnnotationFlags{};
    request.controls = enabled;
    request.chips = false;
    request.legend = m_legend;
    request.ghost = ghost;
    request.linked = enabled ? action_destination(selected, linked_action(m_model)) : k_no_space;
    m_graphics_board->set_annotations(request);
    m_pushed_flags = request.flags;
}

void Dashboard::status()
{
    const uint32_t decisions = static_cast<uint32_t>(m_feed.size());
    gui::label(gui::context().arena().format("Match %u \xC2\xB7 %u %s", m_feed.match_index() + 1, decisions, decisions == 1 ? "decision" : "decisions"));
}

void Dashboard::menu()
{
    gui::separator();
    if (gui::menu_item("Board heat", m_flags.heat).clicked)
    {
        m_flags.heat = !m_flags.heat;
    }
    if (gui::menu_item("Chosen move", m_flags.chosen).clicked)
    {
        m_flags.chosen = !m_flags.chosen;
    }
    if (gui::menu_item("Move values", m_flags.value).clicked)
    {
        m_flags.value = !m_flags.value;
    }
    gui::separator();
}

} // namespace oryx
