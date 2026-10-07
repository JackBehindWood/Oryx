#pragma once

#include "Oryx/Interface/Canvas/DrawList.h"
#include "Oryx/Interface/Canvas/LayoutStyle.h"
#include "Oryx/Interface/Canvas/StateTable.h"

namespace oryx
{

class Font;

// What a box draws once it has a rect: an optional rounded fill and outline, and one line of text inside its padding. Views must outlive the solve (store them in the frame arena).
struct BoxPaint
{
    bool has_fill = false;
    Colour fill;
    CornerRadius radius;
    float border_width = 0.0f;
    Colour border;
    std::string_view text;
    float text_height = 16.0f;
    Colour text_colour = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextAlign text_align = TextAlign::Left;
    bool ellipsis = false;
};

struct LayoutNode
{
    Id id;
    std::string_view name;
    LayoutStyle style;
    BoxPaint paint;
    int32_t parent = -1;
    int32_t first_child = -1;
    int32_t last_child = -1;
    int32_t next = -1;
    Vec2f size{ 0.0f, 0.0f };
    Rect rect;
};

// Width of one line of text; a null `width` measures everything as zero (no font yet).
struct TextMeasure
{
    float (*width)(void* user, std::string_view text, float pixel_height) = nullptr;
    void* user = nullptr;
};

// A box tree rebuilt every frame and solved once at its end, in the manner of Clay: fit sizes bottom-up, then grow/compress, alignment and floating placement top-down.
// Nodes sit in one vector that keeps its capacity across frames, so a warm tree allocates nothing. Needs no font and no renderer; text sizes come from a TextMeasure.
// Boxes with an id leave their solved rect behind for the next frame, which is what hit-testing reads (`rect_of`). Main thread only.
class LayoutTree
{
public:
    // Drops the boxes (not the remembered rects). Also recovers after an error.
    void clear();

    // Returns the node index. end_box throws Error when no box is open.
    uint32_t begin_box(Id id, const LayoutStyle& style, std::string_view name = {});
    void end_box();
    uint32_t leaf(Id id, const LayoutStyle& style, std::string_view name = {});

    [[nodiscard]] LayoutNode& node(uint32_t index) { return m_nodes[index]; }
    [[nodiscard]] const LayoutNode& node(uint32_t index) const { return m_nodes[index]; }
    // The innermost open box, or the box last created when none is open. Throws Error on an empty tree.
    [[nodiscard]] LayoutNode& current();
    [[nodiscard]] uint32_t node_count() const { return static_cast<uint32_t>(m_nodes.size()); }
    [[nodiscard]] uint32_t open_depth() const { return static_cast<uint32_t>(m_open.size()); }

    // Resolves every rect inside `viewport` and remembers the rects of boxes with an id under `frame`. Throws Error while a box is open or when a floating box names a target that is not laid out before it.
    void solve(const Rect& viewport, const TextMeasure& measure, uint64_t frame);

    // The rect the box with this id had after the latest solve (so the previous frame's while the current frame is still being built); false when it was not there.
    [[nodiscard]] bool rect_of(Id id, Rect& out) const;

    // Records fills, outlines, text and clips into the list, boxes with a channel on that channel. Text is skipped without a font.
    void paint(DrawList& list, Font* font, float scale) const;

private:
    void arrange(uint32_t index);
    void place_floating(uint32_t index, const Rect& viewport);

    std::vector<LayoutNode> m_nodes;
    std::vector<uint32_t> m_open;
    StateTable<Rect> m_rects;
};

// One line per box in tree order, indented by depth, with fixed two-decimal numbers, for golden tests and debugging.
[[nodiscard]] std::string dump_layout(const LayoutTree& tree);

} // namespace oryx
