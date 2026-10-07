#pragma once

#include "Oryx/Interface/GUI/GuiPolicies.h"
#include "Oryx/Interface/GUI/GuiWidgets.h"

// Widgets that turn numbers into pixels. Data comes in as `Values` (a view of the caller's floats), the look from the theme; everything custom is the caller's own code between the calls of a scope.
namespace oryx::gui
{

// Alpha zero asks for the next colour of the theme's palette.
inline constexpr Colour k_theme_colour = { 0.0f, 0.0f, 0.0f, 0.0f };

struct BarOptions : FieldOptions
{
    NumberFormat format;
    // Colours the bar by its value when set; not owned, read during the call.
    const ColourScale* scale = nullptr;
    // Printed instead of the formatted value when not empty.
    std::string_view value_text;
    bool show_value = true;
};

static_assert(std::is_trivially_copyable_v<BarOptions>);

// A labelled bar filled to value / max; the value is clamped to 0..max and a max of zero draws it empty.
void bar(std::string_view label, float value, float max, const BarOptions& options = {});

enum class PlotStyle : uint8_t
{
    Line,
    Step,
    // A translucent fill down to zero under the line.
    Area,
    Points
};

struct PlotOptions : WidgetOptions
{
    Sizing width = grow();
    Sizing height = fixed(120.0f);
    // A fixed y range; NaN fits the data (taken from the previous frame's series).
    float y_min = std::numeric_limits<float>::quiet_NaN();
    float y_max = std::numeric_limits<float>::quiet_NaN();
    bool frame = true;
    bool legend = true;
    // The top and bottom of the y range written in the corners.
    bool range_labels = true;
};

static_assert(std::is_trivially_copyable_v<PlotOptions>);

struct PlotResult
{
    ItemState item;
    // The sample under the pointer, k_no_index when it is elsewhere or there is no data.
    uint32_t hover_index = k_no_index;
};

static_assert(std::is_trivially_copyable_v<PlotResult> && std::is_standard_layout_v<PlotResult>);

// A plot in the manner of ImPlot: open the scope, add series and markers with calls, leave it. Sample i occupies [i, i + 1) on x and is drawn at its centre, so lines, bars and markers line up.
// The y range and x extent are the previous frame's (a plot is one frame late, like every hit area), so it is blank on the first frame. A pixel column draws at most the min and max of its samples, so a series of any length costs at most two points per column.
// NaN and infinite samples are gaps. Holds at most k_palette_size series (each line()/bars()); a further one throws Error. The scope's painter is clipped to the data rectangle: the legend sits in a row above it and the y range labels in a gutter left of it, so neither covers a series or a marker.
class PlotScope
{
public:
    PlotScope(std::string_view name, const PlotOptions& options = {});
    ~PlotScope();

    PlotScope(const PlotScope&) = delete;
    PlotScope& operator=(const PlotScope&) = delete;

    void line(std::string_view label, const Values& values, PlotStyle style = PlotStyle::Line, const Colour& colour = k_theme_colour);
    // One bar per sample from zero; the y range includes zero.
    void bars(std::string_view label, const Values& values, const Colour& colour = k_theme_colour);
    // A vertical marker through sample `index` and a horizontal one at `value`: thresholds, the selected ply.
    void vline(float index, const Colour& colour, float thickness = 1.0f);
    void hline(float value, const Colour& colour, float thickness = 1.0f);

    // For drawing of your own: the data rectangle (see to_screen) and a painter clipped to the plot.
    [[nodiscard]] PlotArea area() const;
    [[nodiscard]] Painter& painter() { return m_canvas.painter; }
    [[nodiscard]] const ItemState& item() const { return m_canvas.item; }
    [[nodiscard]] PlotResult result() const;

private:
    // What the plot remembers from the frame before.
    struct Memory
    {
        float y_min = 0.0f;
        float y_max = 1.0f;
        uint32_t x_count = 0;
        uint32_t series = 0;
        bool bars = false;
        bool valid = false;
    };

    struct Entry
    {
        std::string_view name;
        Colour colour;
    };

    void draw(const Values& values, PlotStyle style, bool bars, const Colour& colour);
    [[nodiscard]] Colour series_colour(const Colour& requested) const;

    GuiContext& m_context;
    const ImStyle* m_style;
    PlotOptions m_options;
    ImId m_id;
    Memory m_previous;
    im::CanvasArea m_canvas;
    // The frame inside the canvas minus the legend row above and the label gutter on the left.
    Rect m_inner;
    Rect m_data;
    bool m_clipped = false;
    float m_seen_min = std::numeric_limits<float>::max();
    float m_seen_max = std::numeric_limits<float>::lowest();
    uint32_t m_seen_count = 0;
    bool m_seen_bars = false;
    uint32_t m_series = 0;
    Entry m_entries[k_palette_size];
};

// One-call forms of a single-series plot.
PlotResult plot_lines(std::string_view name, const Values& values, const PlotOptions& options = {}, PlotStyle style = PlotStyle::Line, const Colour& colour = k_theme_colour);
// One bar per value (a policy, a histogram the caller has already binned).
PlotResult plot_histogram(std::string_view name, const Values& values, const PlotOptions& options = {}, const Colour& colour = k_theme_colour);

struct SparklineOptions : WidgetOptions
{
    Sizing width = fixed(80.0f);
    Sizing height = fixed(24.0f);
    float y_min = std::numeric_limits<float>::quiet_NaN();
    float y_max = std::numeric_limits<float>::quiet_NaN();
    Colour colour = k_theme_colour;
};

static_assert(std::is_trivially_copyable_v<SparklineOptions>);

// A small line with no frame, legend or labels, for a table cell or a stat row.
PlotResult sparkline(std::string_view name, const Values& values, const SparklineOptions& options = {});

struct HeatCellOptions : WidgetOptions
{
    Sizing width = fixed(24.0f);
    Sizing height = fixed(24.0f);
    NumberFormat format;
    bool show_value = false;
};

static_assert(std::is_trivially_copyable_v<HeatCellOptions>);

[[nodiscard]] ItemState heat_cell(std::string_view label, float value, const ColourScale& scale, const HeatCellOptions& options = {});

struct HeatGridOptions : WidgetOptions
{
    Vec2f cell{ 24.0f, 24.0f };
    float gap = 2.0f;
    NumberFormat format;
    bool show_values = false;
};

static_assert(std::is_trivially_copyable_v<HeatGridOptions>);

struct HeatGridResult
{
    uint32_t hovered_index = k_no_index;
    // The cell released over on this frame.
    uint32_t clicked_index = k_no_index;
};

static_assert(std::is_trivially_copyable_v<HeatGridResult> && std::is_standard_layout_v<HeatGridResult>);

// `values` laid out row by row, `columns` to a row, each cell coloured by the scale; fits its cells, so a board-shaped policy is a heat grid of its width.
HeatGridResult heat_grid(std::string_view name, const Values& values, uint32_t columns, const ColourScale& scale, const HeatGridOptions& options = {});

// A swatch followed by its name, for a legend of your own (a plot draws its own from its series).
void legend_item(std::string_view name, const Colour& colour, const WidgetOptions& options = {});

} // namespace oryx::gui
