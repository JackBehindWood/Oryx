#include "BoardRenderer2D.h"

namespace oryx
{

namespace
{

constexpr float k_label_height = 16.0f;
constexpr float k_target_dot = 0.16f;
constexpr float k_ring_thickness = 0.22f;
constexpr float k_cross_thickness = 0.16f;
constexpr float k_outline = 1.08f;

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

void draw_board_2d(BatchRenderer2D& batcher, const BoardScene& scene, const BoardProjection2D& layout, const BoardTheme2D& theme, Font& font)
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

    draw_axis_labels(batcher, board, layout, theme, font);
}

} // namespace oryx
