#include "oxpch.h"
#include "Oryx/Interface/Canvas/LayoutTree.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Interface/Canvas/FrameArena.h"
#include "Oryx/Interface/Canvas/Painter.h"

namespace oryx
{

namespace
{

constexpr float k_epsilon = 1.0e-3f;

float clamp_to(const Sizing& sizing, float value)
{
    return std::min(std::max(value, sizing.min), sizing.max);
}

const Sizing& sizing_of(const LayoutNode& node, uint32_t axis)
{
    return axis == 0 ? node.style.width : node.style.height;
}

uint32_t main_axis(const LayoutNode& node)
{
    return node.style.direction == Direction::Row ? 0u : 1u;
}

Align align_on(const LayoutNode& node, uint32_t axis)
{
    return axis == 0 ? node.style.align_x : node.style.align_y;
}

float padding_on(const LayoutNode& node, uint32_t axis)
{
    const Insets& padding = node.style.padding;
    return axis == 0 ? padding.left + padding.right : padding.top + padding.bottom;
}

bool in_flow(const LayoutNode& node)
{
    return !node.style.floating.enabled;
}

bool is_definite(SizingKind kind)
{
    return kind == SizingKind::Fixed || kind == SizingKind::Percent;
}

float aligned_offset(Align align, float free)
{
    free = std::max(free, 0.0f);
    return align == Align::Centre ? free * 0.5f : align == Align::End ? free : 0.0f;
}

Vec2f point_on(const Rect& rect, AttachPoint point)
{
    const uint32_t index = static_cast<uint32_t>(point);
    return Vec2f(rect.min[0] + rect.size[0] * 0.5f * static_cast<float>(index % 3), rect.min[1] + rect.size[1] * 0.5f * static_cast<float>(index / 3));
}

// Derives the missing axis from a definite one, or fits a box into two flexible axes.
void apply_aspect(LayoutNode& node)
{
    const float ratio = node.style.aspect_ratio;
    if (!(ratio > 0.0f))
    {
        return;
    }
    const SizingKind width = node.style.width.kind;
    const SizingKind height = node.style.height.kind;
    if (is_definite(width) && height == SizingKind::Fit)
    {
        node.size[1] = clamp_to(node.style.height, node.size[0] / ratio);
    }
    else if (is_definite(height) && width == SizingKind::Fit)
    {
        node.size[0] = clamp_to(node.style.width, node.size[1] * ratio);
    }
    else if (!is_definite(width) && !is_definite(height))
    {
        const float fitted_width = std::min(node.size[0], node.size[1] * ratio);
        node.size[0] = clamp_to(node.style.width, fitted_width);
        node.size[1] = clamp_to(node.style.height, fitted_width / ratio);
    }
}

void compress(LayoutTree& tree, const LayoutNode& parent, uint32_t axis, float overflow)
{
    for (uint32_t pass = 0; pass < 64 && overflow > k_epsilon; ++pass)
    {
        float largest = 0.0f;
        float second = 0.0f;
        for (int32_t child = parent.first_child; child >= 0; child = tree.node(static_cast<uint32_t>(child)).next)
        {
            const LayoutNode& node = tree.node(static_cast<uint32_t>(child));
            const float size = node.size[axis];
            if (!in_flow(node) || is_definite(sizing_of(node, axis).kind) || size <= sizing_of(node, axis).min + k_epsilon)
            {
                continue;
            }
            if (size > largest + k_epsilon)
            {
                second = largest;
                largest = size;
            }
            else if (size < largest - k_epsilon)
            {
                second = std::max(second, size);
            }
        }
        if (largest <= 0.0f)
        {
            return;
        }
        uint32_t count = 0;
        for (int32_t child = parent.first_child; child >= 0; child = tree.node(static_cast<uint32_t>(child)).next)
        {
            const LayoutNode& node = tree.node(static_cast<uint32_t>(child));
            count += in_flow(node) && !is_definite(sizing_of(node, axis).kind) && std::abs(node.size[axis] - largest) <= k_epsilon ? 1u : 0u;
        }
        const float step = std::min(overflow / static_cast<float>(count), largest - second);
        float removed = 0.0f;
        for (int32_t child = parent.first_child; child >= 0; child = tree.node(static_cast<uint32_t>(child)).next)
        {
            LayoutNode& node = tree.node(static_cast<uint32_t>(child));
            if (in_flow(node) && !is_definite(sizing_of(node, axis).kind) && std::abs(node.size[axis] - largest) <= k_epsilon)
            {
                const float reduced = std::max(node.size[axis] - step, sizing_of(node, axis).min);
                removed += node.size[axis] - reduced;
                node.size[axis] = reduced;
            }
        }
        if (removed <= 0.0f)
        {
            return;
        }
        overflow -= removed;
    }
}

void distribute(LayoutTree& tree, const LayoutNode& parent, uint32_t axis, float remaining)
{
    for (uint32_t pass = 0; pass < 64 && remaining > k_epsilon; ++pass)
    {
        float weights = 0.0f;
        for (int32_t child = parent.first_child; child >= 0; child = tree.node(static_cast<uint32_t>(child)).next)
        {
            const LayoutNode& node = tree.node(static_cast<uint32_t>(child));
            const Sizing& sizing = sizing_of(node, axis);
            if (in_flow(node) && sizing.kind == SizingKind::Grow && node.size[axis] < sizing.max - k_epsilon)
            {
                weights += sizing.value;
            }
        }
        if (!(weights > 0.0f))
        {
            return;
        }
        float given = 0.0f;
        for (int32_t child = parent.first_child; child >= 0; child = tree.node(static_cast<uint32_t>(child)).next)
        {
            LayoutNode& node = tree.node(static_cast<uint32_t>(child));
            const Sizing& sizing = sizing_of(node, axis);
            if (in_flow(node) && sizing.kind == SizingKind::Grow && node.size[axis] < sizing.max - k_epsilon)
            {
                const float grown = std::min(node.size[axis] + remaining * sizing.value / weights, sizing.max);
                given += grown - node.size[axis];
                node.size[axis] = grown;
            }
        }
        if (given <= 0.0f)
        {
            return;
        }
        remaining -= given;
    }
}

void write_sizing(std::string& out, const char* axis, const Sizing& sizing)
{
    char buffer[64];
    switch (sizing.kind)
    {
    case SizingKind::Fit:
        snprintf_c(buffer, sizeof(buffer), " %s=fit", axis);
        break;
    case SizingKind::Grow:
        snprintf_c(buffer, sizeof(buffer), " %s=grow(%.2f)", axis, sizing.value);
        break;
    case SizingKind::Fixed:
        snprintf_c(buffer, sizeof(buffer), " %s=fixed(%.2f)", axis, sizing.value);
        break;
    case SizingKind::Percent:
        snprintf_c(buffer, sizeof(buffer), " %s=percent(%.2f)", axis, sizing.value);
        break;
    }
    out += buffer;
}

void dump_node(const LayoutTree& tree, uint32_t index, uint32_t depth, std::string& out)
{
    const LayoutNode& node = tree.node(index);
    char buffer[160];
    out.append(depth * 2, ' ');
    if (!node.name.empty())
    {
        out += node.name;
    }
    else
    {
        snprintf_c(buffer, sizeof(buffer), "#%016llx", static_cast<unsigned long long>(node.id.value));
        out += buffer;
    }
    snprintf_c(buffer, sizeof(buffer), " [%.2f %.2f %.2f %.2f] %s", node.rect.min[0], node.rect.min[1], node.rect.size[0], node.rect.size[1], node.style.direction == Direction::Row ? "row" : "column");
    out += buffer;
    write_sizing(out, "w", node.style.width);
    write_sizing(out, "h", node.style.height);
    if (node.style.floating.enabled)
    {
        out += " floating";
    }
    if (node.style.overflow == Overflow::Clip)
    {
        out += " clip";
    }
    if (!node.paint.text.empty())
    {
        out += " text=\"";
        out += node.paint.text;
        out += "\"";
    }
    out += '\n';
    for (int32_t child = node.first_child; child >= 0; child = tree.node(static_cast<uint32_t>(child)).next)
    {
        dump_node(tree, static_cast<uint32_t>(child), depth + 1, out);
    }
}

struct PaintPass
{
    const LayoutTree& tree;
    DrawList& list;
    Painter* painter;
    float scale;
};

void paint_node(const PaintPass& pass, uint32_t index, uint32_t inherited_channel)
{
    const LayoutNode& node = pass.tree.node(index);
    const uint32_t channel = node.style.channel != 0 ? node.style.channel : inherited_channel;
    pass.list.set_channel(channel);
    const BoxPaint& paint = node.paint;
    if (paint.has_fill)
    {
        const Rect snapped = snap_to_pixels(node.rect, pass.scale);
        if (is_square(paint.radius))
        {
            pass.list.add_rect(snapped, paint.fill);
        }
        else
        {
            pass.list.add_rounded_rect(snapped, paint.radius, paint.fill);
        }
    }
    if (paint.border_width > 0.0f)
    {
        pass.list.add_border(snap_to_pixels(node.rect, pass.scale), paint.radius, paint.border_width, paint.border);
    }
    if (pass.painter != nullptr && !paint.text.empty())
    {
        pass.painter->text(inset(node.rect, node.style.padding), paint.text, { paint.text_height, paint.text_colour, paint.text_align, paint.ellipsis });
    }
    const bool clips = node.style.overflow == Overflow::Clip;
    if (clips)
    {
        pass.list.push_clip(node.rect);
    }
    for (int32_t child = node.first_child; child >= 0; child = pass.tree.node(static_cast<uint32_t>(child)).next)
    {
        paint_node(pass, static_cast<uint32_t>(child), channel);
    }
    if (clips)
    {
        pass.list.pop_clip();
    }
    pass.list.set_channel(inherited_channel);
}

} // namespace

void LayoutTree::clear()
{
    m_nodes.clear();
    m_open.clear();
}

uint32_t LayoutTree::begin_box(ImId id, const LayoutStyle& style, std::string_view name)
{
    const uint32_t index = static_cast<uint32_t>(m_nodes.size());
    LayoutNode node;
    node.id = id;
    node.name = name;
    node.style = style;
    node.parent = m_open.empty() ? -1 : static_cast<int32_t>(m_open.back());
    m_nodes.push_back(node);
    if (node.parent >= 0)
    {
        LayoutNode& parent = m_nodes[static_cast<uint32_t>(node.parent)];
        if (parent.last_child >= 0)
        {
            m_nodes[static_cast<uint32_t>(parent.last_child)].next = static_cast<int32_t>(index);
        }
        else
        {
            parent.first_child = static_cast<int32_t>(index);
        }
        parent.last_child = static_cast<int32_t>(index);
    }
    m_open.push_back(index);
    return index;
}

void LayoutTree::end_box()
{
    if (m_open.empty())
    {
        throw Error("LayoutTree has no box to end", "every end_box needs a begin_box");
    }
    m_open.pop_back();
}

uint32_t LayoutTree::leaf(ImId id, const LayoutStyle& style, std::string_view name)
{
    const uint32_t index = begin_box(id, style, name);
    end_box();
    return index;
}

LayoutNode& LayoutTree::current()
{
    if (m_nodes.empty())
    {
        throw Error("LayoutTree has no box", "begin_box first");
    }
    return m_open.empty() ? m_nodes.back() : m_nodes[m_open.back()];
}

bool LayoutTree::rect_of(ImId id, Rect& out) const
{
    const Rect* found = m_rects.find(id);
    if (found == nullptr)
    {
        return false;
    }
    out = *found;
    return true;
}

void LayoutTree::solve(const Rect& viewport, const TextMeasure& measure, uint64_t frame)
{
    if (!m_open.empty())
    {
        throw Error("LayoutTree solved with a box open", "every begin_box needs an end_box");
    }
    for (uint32_t index = node_count(); index-- > 0;)
    {
        LayoutNode& node = m_nodes[index];
        Vec2f content(padding_on(node, 0), padding_on(node, 1));
        if (!node.paint.text.empty())
        {
            const float width = measure.width != nullptr ? measure.width(measure.user, node.paint.text, node.paint.text_height) : 0.0f;
            content = Vec2f(content[0] + width, content[1] + node.paint.text_height);
        }
        const uint32_t axis_main = main_axis(node);
        const uint32_t axis_cross = 1 - axis_main;
        float main_total = 0.0f;
        float cross_max = 0.0f;
        uint32_t children = 0;
        for (int32_t child = node.first_child; child >= 0; child = m_nodes[static_cast<uint32_t>(child)].next)
        {
            const LayoutNode& other = m_nodes[static_cast<uint32_t>(child)];
            if (in_flow(other))
            {
                main_total += other.size[axis_main];
                cross_max = std::max(cross_max, other.size[axis_cross]);
                ++children;
            }
        }
        if (children > 0)
        {
            content[axis_main] += main_total + node.style.gap * static_cast<float>(children - 1);
            content[axis_cross] += cross_max;
        }
        for (uint32_t axis = 0; axis < 2; ++axis)
        {
            const Sizing& sizing = sizing_of(node, axis);
            node.size[axis] = sizing.kind == SizingKind::Fixed ? clamp_to(sizing, sizing.value)
                : sizing.kind == SizingKind::Percent           ? clamp_to(sizing, 0.0f)
                                                               : clamp_to(sizing, content[axis]);
        }
        apply_aspect(node);
    }

    for (uint32_t index = 0; index < node_count(); ++index)
    {
        LayoutNode& node = m_nodes[index];
        if (node.parent >= 0 || !in_flow(node))
        {
            continue;
        }
        for (uint32_t axis = 0; axis < 2; ++axis)
        {
            const Sizing& sizing = sizing_of(node, axis);
            if (sizing.kind == SizingKind::Grow)
            {
                node.size[axis] = clamp_to(sizing, viewport.size[axis]);
            }
            else if (sizing.kind == SizingKind::Percent)
            {
                node.size[axis] = clamp_to(sizing, sizing.value * viewport.size[axis]);
            }
        }
        apply_aspect(node);
        node.rect = { viewport.min, node.size };
        arrange(index);
    }
    for (uint32_t index = 0; index < node_count(); ++index)
    {
        if (m_nodes[index].style.floating.enabled)
        {
            place_floating(index, viewport);
        }
    }

    for (const LayoutNode& node : m_nodes)
    {
        if (is_valid(node.id))
        {
            m_rects.get(node.id, frame) = node.rect;
        }
    }
    m_rects.collect(frame);
}

void LayoutTree::arrange(uint32_t index)
{
    LayoutNode& parent = m_nodes[index];
    const Insets& padding = parent.style.padding;
    const Vec2f inner_min(parent.rect.min[0] + padding.left, parent.rect.min[1] + padding.top);
    const Vec2f inner_size(std::max(0.0f, parent.rect.size[0] - padding_on(parent, 0)), std::max(0.0f, parent.rect.size[1] - padding_on(parent, 1)));
    const uint32_t axis_main = main_axis(parent);
    const uint32_t axis_cross = 1 - axis_main;

    uint32_t children = 0;
    for (int32_t child = parent.first_child; child >= 0; child = m_nodes[static_cast<uint32_t>(child)].next)
    {
        LayoutNode& node = m_nodes[static_cast<uint32_t>(child)];
        if (!in_flow(node))
        {
            continue;
        }
        ++children;
        for (uint32_t axis = 0; axis < 2; ++axis)
        {
            const Sizing& sizing = sizing_of(node, axis);
            if (sizing.kind == SizingKind::Percent)
            {
                node.size[axis] = clamp_to(sizing, sizing.value * inner_size[axis]);
            }
        }
        const Sizing& cross = sizing_of(node, axis_cross);
        if (cross.kind == SizingKind::Grow)
        {
            node.size[axis_cross] = clamp_to(cross, inner_size[axis_cross]);
        }
    }
    if (children == 0)
    {
        return;
    }

    const float gaps = parent.style.gap * static_cast<float>(children - 1);
    float used = gaps;
    for (int32_t child = parent.first_child; child >= 0; child = m_nodes[static_cast<uint32_t>(child)].next)
    {
        used += in_flow(m_nodes[static_cast<uint32_t>(child)]) ? m_nodes[static_cast<uint32_t>(child)].size[axis_main] : 0.0f;
    }
    if (used > inner_size[axis_main] + k_epsilon)
    {
        compress(*this, parent, axis_main, used - inner_size[axis_main]);
    }
    else if (used < inner_size[axis_main] - k_epsilon)
    {
        distribute(*this, parent, axis_main, inner_size[axis_main] - used);
    }

    used = gaps;
    for (int32_t child = parent.first_child; child >= 0; child = m_nodes[static_cast<uint32_t>(child)].next)
    {
        LayoutNode& node = m_nodes[static_cast<uint32_t>(child)];
        if (in_flow(node))
        {
            apply_aspect(node);
            used += node.size[axis_main];
        }
    }

    float cursor = aligned_offset(align_on(parent, axis_main), inner_size[axis_main] - used);
    for (int32_t child = parent.first_child; child >= 0; child = m_nodes[static_cast<uint32_t>(child)].next)
    {
        LayoutNode& node = m_nodes[static_cast<uint32_t>(child)];
        if (!in_flow(node))
        {
            continue;
        }
        Vec2f position = inner_min;
        position[axis_main] += cursor;
        position[axis_cross] += aligned_offset(align_on(parent, axis_cross), inner_size[axis_cross] - node.size[axis_cross]);
        node.rect = { position, node.size };
        cursor += node.size[axis_main] + parent.style.gap;
        arrange(static_cast<uint32_t>(child));
    }
}

void LayoutTree::place_floating(uint32_t index, const Rect& viewport)
{
    LayoutNode& node = m_nodes[index];
    const Floating& floating = node.style.floating;
    Rect target = viewport;
    if (floating.target == FloatTarget::Parent && node.parent >= 0)
    {
        target = m_nodes[static_cast<uint32_t>(node.parent)].rect;
    }
    else if (floating.target == FloatTarget::Element)
    {
        bool found = false;
        for (uint32_t other = 0; other < index && !found; ++other)
        {
            if (m_nodes[other].id == floating.target_id)
            {
                target = m_nodes[other].rect;
                found = true;
            }
        }
        if (!found)
        {
            throw Error("LayoutTree floating box names a target that is not laid out before it", "create the target box earlier in the tree");
        }
    }
    for (uint32_t axis = 0; axis < 2; ++axis)
    {
        const Sizing& sizing = sizing_of(node, axis);
        if (sizing.kind == SizingKind::Grow)
        {
            node.size[axis] = clamp_to(sizing, target.size[axis]);
        }
        else if (sizing.kind == SizingKind::Percent)
        {
            node.size[axis] = clamp_to(sizing, sizing.value * target.size[axis]);
        }
    }
    apply_aspect(node);
    const Vec2f anchor = point_on(target, floating.target_point);
    const Vec2f pivot = point_on({ { 0.0f, 0.0f }, node.size }, floating.element);
    node.rect = { Vec2f(anchor[0] - pivot[0] + floating.offset[0], anchor[1] - pivot[1] + floating.offset[1]), node.size };
    arrange(index);
}

void LayoutTree::paint(DrawList& list, Font* font, float scale) const
{
    uint32_t channels = 1;
    for (const LayoutNode& node : m_nodes)
    {
        channels = std::max(channels, node.style.channel + 1);
    }
    const uint32_t previous_channels = list.channel_count();
    if (channels > previous_channels)
    {
        list.split_channels(channels);
    }
    const auto paint_roots = [&](Painter* painter)
    {
        const PaintPass pass{ *this, list, painter, scale };
        for (uint32_t index = 0; index < node_count(); ++index)
        {
            if (m_nodes[index].parent < 0)
            {
                paint_node(pass, index, 0);
            }
        }
    };
    if (font != nullptr)
    {
        Painter painter(list, *font, scale);
        paint_roots(&painter);
    }
    else
    {
        paint_roots(nullptr);
    }
    if (channels > previous_channels)
    {
        list.merge();
    }
}

std::string dump_layout(const LayoutTree& tree)
{
    std::string out;
    for (uint32_t index = 0; index < tree.node_count(); ++index)
    {
        if (tree.node(index).parent < 0)
        {
            dump_node(tree, index, 0, out);
        }
    }
    return out;
}

} // namespace oryx
