#include "BoardRenderer2D.h"

namespace oryx
{

namespace
{

constexpr float k_label_height = 16.0f;
constexpr float k_target_dot = 0.16f;
constexpr float k_ring_thickness = 0.17f;
constexpr float k_cross_thickness = 0.13f;
constexpr float k_outline = 1.08f;
constexpr float k_chosen_ring = 0.86f;
constexpr float k_chosen_ring_thickness = 0.11f;
constexpr float k_arrow_thickness = 0.07f;
constexpr float k_arrow_min_thickness = 3.0f;
constexpr float k_arrow_head = 0.30f;
constexpr float k_link_thickness = 2.5f;
constexpr float k_chip_height = 20.0f;
constexpr float k_chip_padding = 12.0f;
constexpr float k_chip_text = 15.0f;
constexpr float k_heat_text = 12.0f;
constexpr float k_heat_text_inset = 3.0f;
constexpr float k_heat_pill_padding = 3.0f;
constexpr float k_chip_inset = 3.0f;

void draw_text_centred(BatchRenderer2D& batcher, Font& font, const Vec2f& centre, const std::string& text, float pixel_height, const Colour& colour)
{
    if (!font.ready() || text.empty())
    {
        return;
    }
    TextStyle style;
    style.pixel_height = pixel_height;
    style.colour = colour;
    style.align = TextAlign::Centre;
    batcher.draw_text({ centre[0], centre[1] - font.ascent(pixel_height) * 0.5f }, text, font, style);
}

void draw_piece(BatchRenderer2D& batcher, const PieceStyle& style, const Vec2f& centre, float size, const BoardTheme2D& theme)
{
    float radius = size * 0.5f;
    switch (style.shape)
    {
        case PieceShape::Disc:
            batcher.draw_circle(centre, radius * k_outline, theme.piece_outline);
            batcher.draw_circle(centre, radius, style.colour);
            break;
        case PieceShape::Ring:
            batcher.draw_circle(centre, radius, style.colour, k_ring_thickness);
            break;
        case PieceShape::Cross:
            batcher.draw_rect(centre, { size * k_cross_thickness, size * 1.15f }, style.colour, math::PI<float> * 0.25f);
            batcher.draw_rect(centre, { size * k_cross_thickness, size * 1.15f }, style.colour, -math::PI<float> * 0.25f);
            break;
        case PieceShape::Square:
            batcher.draw_rect(centre, { size * k_outline, size * k_outline }, theme.piece_outline);
            batcher.draw_rect(centre, { size, size }, style.colour);
            break;
    }
}

void draw_space(BatchRenderer2D& batcher, const BoardLayout2D& board, SpaceId space, SpaceHighlight highlight, const BoardProjection2D& layout, const BoardTheme2D& theme)
{
    Vec2f centre = board_to_world(layout, board.position(space));
    Vec2f size = { board.size(space)[0] * layout.scale * (1.0f - theme.space_gap), board.size(space)[1] * layout.scale * (1.0f - theme.space_gap) };
    float radius = std::min(size[0], size[1]) * 0.5f;

    auto fill = [&](const Colour& colour)
    {
        if (board.shape(space) == SpaceShape::Circle)
        {
            batcher.draw_circle(centre, radius, colour);
        }
        else
        {
            batcher.draw_rect(centre, size, colour);
        }
    };

    fill(theme.tones[board.tone(space) % 2]);
    if (has_highlight(highlight, SpaceHighlight::Changed))
    {
        fill(theme.changed);
    }
    if (has_highlight(highlight, SpaceHighlight::Picked))
    {
        fill(theme.picked);
    }
    if (has_highlight(highlight, SpaceHighlight::Hover))
    {
        fill(theme.hover);
    }
}

void draw_axis_labels(BatchRenderer2D& batcher, const BoardLayout2D& board, const BoardProjection2D& layout, const BoardTheme2D& theme, Font& font)
{
    const std::vector<float>& columns = board.columns();
    const std::vector<float>& rows = board.rows();
    if (board.column_labels().size() == columns.size())
    {
        float y = board_to_world(layout, { 0.0f, board.min()[1] })[1] - k_board_gutter * 0.5f;
        for (size_t index = 0; index < columns.size(); ++index)
        {
            draw_text_centred(batcher, font, { board_to_world(layout, { columns[index], 0.0f })[0], y }, board.column_labels()[index], k_label_height, theme.label);
        }
    }
    if (board.row_labels().size() == rows.size())
    {
        float x = board_to_world(layout, { board.min()[0], 0.0f })[0] - k_board_gutter * 0.5f;
        for (size_t index = 0; index < rows.size(); ++index)
        {
            draw_text_centred(batcher, font, { x, board_to_world(layout, { 0.0f, rows[index] })[1] }, board.row_labels()[rows.size() - 1 - index], k_label_height, theme.label);
        }
    }
}

Vec2f space_size(const BoardLayout2D& board, SpaceId space, const BoardProjection2D& layout)
{
    return { board.size(space)[0] * layout.scale, board.size(space)[1] * layout.scale };
}

bool in_range(const BoardLayout2D& board, SpaceId space)
{
    return space < board.space_count();
}

Colour with_alpha(const Colour& colour, float alpha)
{
    return { colour.r, colour.g, colour.b, alpha };
}

void draw_heat(BatchRenderer2D& batcher, const BoardLayout2D& board, const BoardProjection2D& layout, const BoardTheme2D& theme, const BoardAnnotationLayer& annotations)
{
    for (const BoardAnnotation& item : annotations.annotations->items)
    {
        const SpaceId space = destination(item);
        if (item.kind != AnnotationKind::Heat || !in_range(board, space))
        {
            continue;
        }
        const Vec2f centre = board_to_world(layout, board.position(space));
        const Vec2f size = space_size(board, space, layout) * (1.0f - theme.space_gap);
        const Colour tint = with_alpha(theme.heat, math::lerp(theme.heat_floor, theme.heat_peak, math::clamp(item.weight, 0.0f, 1.0f)));
        if (board.shape(space) == SpaceShape::Circle)
        {
            batcher.draw_circle(centre, math::min(size[0], size[1]) * 0.5f, tint);
        }
        else
        {
            batcher.draw_rect(centre, size, tint);
        }
    }
}

void draw_heat_text(BatchRenderer2D& batcher, const BoardLayout2D& board, const BoardProjection2D& layout, const BoardTheme2D& theme, Font& font, const BoardAnnotationLayer& annotations)
{
    if (!font.ready())
    {
        return;
    }
    TextStyle style;
    style.pixel_height = k_heat_text;
    style.colour = { 0.96f, 0.96f, 0.98f, 1.0f };
    for (const BoardAnnotation& item : annotations.annotations->items)
    {
        const SpaceId space = destination(item);
        if (item.kind != AnnotationKind::Heat || !in_range(board, space) || std::isnan(item.value))
        {
            continue;
        }
        const Vec2f size = space_size(board, space, layout);
        if (math::min(size[0], size[1]) < theme.heat_text_min_size)
        {
            continue;
        }
        const Vec2f centre = board_to_world(layout, board.position(space));
        char text[16];
        std::snprintf(text, sizeof(text), "%.0f%%", static_cast<double>(item.value) * 100.0);
        const Vec2f pill = { font.measure(text, k_heat_text).width + 2.0f * k_heat_pill_padding, k_heat_text + 2.0f * k_heat_pill_padding };
        const Vec2f pill_centre = { centre[0] - size[0] * 0.5f + k_heat_text_inset + pill[0] * 0.5f, centre[1] + size[1] * 0.5f - k_heat_text_inset - pill[1] * 0.5f };
        batcher.draw_rect(pill_centre, pill, theme.heat_pill);
        draw_text_centred(batcher, font, pill_centre, text, k_heat_text, style.colour);
    }
}

void draw_segment(BatchRenderer2D& batcher, const Vec2f& from, const Vec2f& to, float thickness, const Colour& colour)
{
    const Vec2f delta = to - from;
    const float length = std::sqrt(delta[0] * delta[0] + delta[1] * delta[1]);
    if (length <= math::EPSILON<float>)
    {
        return;
    }
    batcher.draw_rect((from + to) * 0.5f, { length, thickness }, colour, std::atan2(delta[1], delta[0]));
}

void draw_chosen(BatchRenderer2D& batcher, const BoardLayout2D& board, const BoardProjection2D& layout, const BoardTheme2D& theme, const BoardAnnotation& item)
{
    const SpaceId last = destination(item);
    if (!in_range(board, last))
    {
        return;
    }
    const Vec2f end = board_to_world(layout, board.position(last));
    const float cell = math::min(board.size(last)[0], board.size(last)[1]) * layout.scale;
    const float radius = cell * 0.5f * k_chosen_ring;
    const float thickness = math::max(k_arrow_min_thickness, cell * k_arrow_thickness);
    Vec2f previous = end;
    bool have_previous = false;
    for (uint8_t step = 0; step < item.path_count; ++step)
    {
        if (!in_range(board, item.path[step]))
        {
            continue;
        }
        const Vec2f point = board_to_world(layout, board.position(item.path[step]));
        if (have_previous)
        {
            draw_segment(batcher, previous, point, thickness, theme.chosen);
        }
        previous = point;
        have_previous = true;
    }
    batcher.draw_circle(end, radius, theme.chosen, k_chosen_ring_thickness);
    if (item.path_count < 2)
    {
        return;
    }
    Vec2f from = end;
    for (int32_t step = static_cast<int32_t>(item.path_count) - 2; step >= 0; --step)
    {
        if (in_range(board, item.path[step]))
        {
            from = board_to_world(layout, board.position(item.path[step]));
            break;
        }
    }
    const Vec2f delta = end - from;
    const float length = std::sqrt(delta[0] * delta[0] + delta[1] * delta[1]);
    if (length <= math::EPSILON<float>)
    {
        return;
    }
    const Vec2f direction = delta * (1.0f / length);
    const Vec2f side = { -direction[1], direction[0] };
    const float head = cell * k_arrow_head;
    const Vec2f tip = end - direction * radius;
    batcher.draw_triangle(tip, tip - direction * head + side * (head * 0.5f), tip - direction * head - side * (head * 0.5f), theme.chosen);
}

void draw_value_chip(BatchRenderer2D& batcher, const BoardLayout2D& board, const BoardProjection2D& layout, const BoardTheme2D& theme, Font& font, const BoardAnnotation& item)
{
    const SpaceId space = destination(item);
    if (!in_range(board, space) || std::isnan(item.value) || !font.ready())
    {
        return;
    }
    char text[24];
    std::snprintf(text, sizeof(text), math::abs(item.value) < 100.0f ? "%+.2f" : "%+.0f", static_cast<double>(item.value));
    const float width = font.measure(text, k_chip_text).width + k_chip_padding;
    const Vec2f size = space_size(board, space, layout);
    const Vec2f centre = board_to_world(layout, board.position(space)) - Vec2f(0.0f, size[1] * 0.5f - k_chip_height * 0.5f - k_chip_inset);
    batcher.draw_rect(centre, { width + 2.0f, k_chip_height + 2.0f }, theme.piece_outline);
    batcher.draw_rect(centre, { width, k_chip_height }, item.value >= 0.0f ? theme.value_positive : theme.value_negative);
    TextStyle style;
    style.pixel_height = k_chip_text;
    style.colour = theme.piece_outline;
    style.align = TextAlign::Centre;
    batcher.draw_text({ centre[0], centre[1] - font.ascent(k_chip_text) * 0.5f }, text, font, style);
}

void draw_link(BatchRenderer2D& batcher, const BoardLayout2D& board, const BoardProjection2D& layout, const BoardTheme2D& theme, SpaceId space)
{
    const Vec2f centre = board_to_world(layout, board.position(space));
    const Vec2f size = space_size(board, space, layout);
    const float t = k_link_thickness;
    batcher.draw_rect({ centre[0], centre[1] + size[1] * 0.5f - t * 0.5f }, { size[0], t }, theme.link);
    batcher.draw_rect({ centre[0], centre[1] - size[1] * 0.5f + t * 0.5f }, { size[0], t }, theme.link);
    batcher.draw_rect({ centre[0] - size[0] * 0.5f + t * 0.5f, centre[1] }, { t, size[1] }, theme.link);
    batcher.draw_rect({ centre[0] + size[0] * 0.5f - t * 0.5f, centre[1] }, { t, size[1] }, theme.link);
}

void draw_annotations_over(BatchRenderer2D& batcher, const BoardLayout2D& board, const BoardProjection2D& layout, const BoardTheme2D& theme, Font& font, const BoardAnnotationLayer& annotations)
{
    if (annotations.flags.heat)
    {
        draw_heat_text(batcher, board, layout, theme, font, annotations);
    }
    for (const BoardAnnotation& item : annotations.annotations->items)
    {
        if (item.kind == AnnotationKind::Chosen && annotations.flags.chosen)
        {
            draw_chosen(batcher, board, layout, theme, item);
        }
        else if (item.kind == AnnotationKind::Value && annotations.flags.value && destination(item) == annotations.hovered)
        {
            draw_value_chip(batcher, board, layout, theme, font, item);
        }
    }
}

} // namespace

UiTheme board_ui_theme()
{
    UiTheme theme;
    theme.base.background = { 0.25f, 0.28f, 0.36f, 1.0f };
    theme.base.hover = { 0.32f, 0.36f, 0.46f, 1.0f };
    theme.base.pressed = { 0.19f, 0.21f, 0.28f, 1.0f };
    theme.base.text = { 1.0f, 1.0f, 1.0f, 1.0f };
    theme.base.radius = 0.0f;
    theme.base.border_width = 0.0f;
    theme.base.text_height = 18.0f;
    theme.status.text = { 1.0f, 1.0f, 1.0f, 1.0f };
    theme.status.text_height = 24.0f;
    theme.status.padding = {};

    ImStyle turn = theme.status;
    turn.text = { 0.55f, 0.92f, 0.60f, 1.0f };
    add_style_variant(theme, "your_turn", turn);
    turn.text = { 0.75f, 0.78f, 0.86f, 1.0f };
    add_style_variant(theme, "their_turn", turn);

    ImStyle banner;
    banner.background = { 0.07f, 0.08f, 0.11f, 0.94f };
    banner.border = { 0.35f, 0.38f, 0.46f, 1.0f };
    banner.border_width = 1.0f;
    banner.radius = 10.0f;
    add_style_variant(theme, "banner", banner);

    ImStyle headline;
    headline.text_height = 40.0f;
    headline.padding = {};
    headline.text = { 0.45f, 0.90f, 0.55f, 1.0f };
    add_style_variant(theme, "win", headline);
    headline.text = { 0.95f, 0.45f, 0.45f, 1.0f };
    add_style_variant(theme, "lose", headline);
    headline.text = { 0.85f, 0.87f, 0.92f, 1.0f };
    add_style_variant(theme, "draw", headline);

    ImStyle hint;
    hint.text_height = 16.0f;
    hint.padding = {};
    hint.text = { 0.60f, 0.62f, 0.70f, 1.0f };
    add_style_variant(theme, "hint", hint);

    ImStyle primary = theme.base;
    primary.background = { 0.22f, 0.50f, 0.88f, 1.0f };
    primary.hover = { 0.30f, 0.58f, 0.95f, 1.0f };
    primary.pressed = { 0.16f, 0.40f, 0.74f, 1.0f };
    primary.radius = 6.0f;
    primary.text_height = 20.0f;
    add_style_variant(theme, "primary", primary);
    return theme;
}

void draw_board_2d(BatchRenderer2D& batcher, const BoardScene& scene, const BoardProjection2D& layout, const BoardTheme2D& theme, Font& font, const BoardAnnotationLayer& annotations)
{
    if (layout.scale <= 0.0f || scene.layout == nullptr)
    {
        return;
    }
    const BoardLayout2D& board = layout_as<BoardLayout2D>(scene.layout, "board");

    for (SpaceId space = 0; space < board.space_count(); ++space)
    {
        draw_space(batcher, board, space, scene.highlights[space], layout, theme);
    }
    const bool annotated = annotations.annotations != nullptr;
    if (annotated && annotations.flags.heat)
    {
        draw_heat(batcher, board, layout, theme, annotations);
    }

    for (const ScenePiece& piece : scene.pieces)
    {
        if (piece.space >= board.space_count())
        {
            continue;
        }
        bool dragged = scene.drag.active && scene.drag.space == piece.space;
        Vec2f centre = dragged ? cursor_to_world(layout, scene.drag.cursor) : board_to_world(layout, board.position(piece.space));
        draw_piece(batcher, piece.style, centre, std::min(board.size(piece.space)[0], board.size(piece.space)[1]) * layout.scale * theme.piece_size, theme);
    }

    for (SpaceId space = 0; space < board.space_count(); ++space)
    {
        if (has_highlight(scene.highlights[space], SpaceHighlight::Target))
        {
            Vec2f centre = board_to_world(layout, board.position(space));
            batcher.draw_circle(centre, std::min(board.size(space)[0], board.size(space)[1]) * layout.scale * k_target_dot, theme.target);
        }
    }

    if (annotated)
    {
        draw_annotations_over(batcher, board, layout, theme, font, annotations);
        if (in_range(board, annotations.linked))
        {
            draw_link(batcher, board, layout, theme, annotations.linked);
        }
    }

    draw_axis_labels(batcher, board, layout, theme, font);
}

} // namespace oryx
