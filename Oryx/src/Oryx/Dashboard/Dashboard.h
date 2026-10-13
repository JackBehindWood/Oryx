#pragma once

#include "Oryx/Board/BoardAnnotation.h"
#include "Oryx/Board/BoardLayer.h"
#include "Oryx/Board/Graphics/IGraphicsBoard.h"
#include "Oryx/Dashboard/DashboardSettings.h"
#include "Oryx/Dashboard/Feed/DashboardFeed.h"
#include "Oryx/Dashboard/Feed/DashboardModel.h"
#include "Oryx/Dashboard/View/DashboardViewRegistry.h"
#include "Oryx/Interface/Workbench/Workbench.h"

namespace oryx
{

// The board's viewport panel: it sits outside the dashboard's group, so closing the dashboard leaves the board.
inline constexpr const char* k_viewport_panel = "viewport/0";

struct DashboardState
{
    bool heat = false;
    bool chosen = false;
    bool value = false;
};

template<typename Visitor>
void reflect(Visitor& v, DashboardState& state)
{
    v.field("heat", state.heat);
    v.field("chosen", state.chosen);
    v.field("value", state.value);
}

// The strategy dashboard as a workbench: one panel per view over a feed the board layer fills with every decision, and the link that shows the selected decision on the board and the board's pointer in the views.
//   Dashboard& dashboard = workbenches.add<Dashboard>();
//   dashboard.add_view("trace");            // by registry id
//   dashboard.add_view<MyView>(arguments);  // or in place
// Add views before the first frame: each registers its panel with placement hints, which a saved layout overrides. Always updates, so a closed dashboard still takes its annotations off the board.
class Dashboard : public Workbench
{
public:
    Dashboard();
    ~Dashboard() noexcept override;

    // The view registered as `id`; an unknown id is logged and skipped.
    void add_view(const std::string& id);
    template<typename V, typename... Args>
    void add_view(Args&&... args)
    {
        static_assert(std::is_base_of_v<IDashboardView, V>, "a dashboard view implements IDashboardView");
        add_view(create_unique<V>(std::forward<Args>(args)...));
    }
    void add_view(UniquePtr<IDashboardView> view);

    // The board layer whose opponent the legend names.
    void bind(const BoardLayer& layer) { m_board = &layer; }
    // A match's board was created or is being released: the dashboard teaches the feed the game's action names and talks annotations to the board.
    void board_created(IGraphicsBoard& board, const std::string& game);
    void board_released(IGraphicsBoard& board);

    void status() override;
    void menu() override;

    [[nodiscard]] DashboardFeed& feed() { return m_feed; }
    [[nodiscard]] DashboardModel& model() { return m_model; }
    [[nodiscard]] const AnnotationFlags& annotation_flags() const { return m_flags; }
    [[nodiscard]] const BoardAnnotations& annotations() const { return m_annotations; }
    [[nodiscard]] size_t view_count() const { return m_views.size(); }

protected:
    void declare_panels() override;
    void draw_panel(PanelHandle panel, const PanelFrame& frame) override;
    void tick(const WorkbenchFrame& frame) override;
    void state_loaded() override;

private:
    void sync_board();
    void apply_settings();

    const BoardLayer* m_board = nullptr;
    DashboardFeed m_feed;
    DashboardModel m_model;
    IGraphicsBoard* m_graphics_board = nullptr;
    BoardAnnotations m_annotations;
    AnnotationFlags m_flags;
    AnnotationFlags m_pushed_flags;
    DashboardState m_saved;
    std::string m_legend;
    uint32_t m_seen_match = 0;
    std::vector<UniquePtr<IDashboardView>> m_views;
    std::string m_side_anchor[5];
    SettingsSubscription m_subscription;
    DashboardSettings m_settings;
    bool m_settings_dirty = false;
};

} // namespace oryx
