#include "BoardRenderer2D.h"

#include "Oryx/Renderer/Renderer.h"

namespace oryx
{

namespace
{

constexpr float k_label_height = 16.0f;
constexpr float k_status_height = 24.0f;
constexpr float k_option_height = 18.0f;
constexpr float k_target_dot = 0.16f;
constexpr float k_ring_thickness = 0.22f;
constexpr float k_cross_thickness = 0.16f;
constexpr float k_outline = 1.08f;

void draw_text_centred(Font& font, const Vec2f& centre, const std::string& text, float pixel_height, const Colour& colour)
{
    if (!font.ready() || text.empty())
    {
        return;
    }
    TextStyle style;
    style.pixel_height = pixel_height;
    style.colour = colour;
    style.align = TextAlign::Centre;
    Renderer::draw_text({ centre[0], centre[1] - font.ascent(pixel_height) * 0.5f }, text, font, style);
}

void draw_piece(const PieceStyle& style, const Vec2f& centre, float size, const BoardTheme2D& theme)
{
    float radius = size * 0.5f;
    switch (style.shape)
    {
        case PieceShape::Disc:
            Renderer::draw_circle(centre, radius * k_outline, theme.piece_outline);
            Renderer::draw_circle(centre, radius, style.colour);
            break;
        case PieceShape::Ring:
            Renderer::draw_circle(centre, radius, style.colour, k_ring_thickness);
            break;
        case PieceShape::Cross:
            Renderer::draw_rect(centre, { size * k_cross_thickness, size * 1.15f }, style.colour, math::PI<float> * 0.25f);
            Renderer::draw_rect(centre, { size * k_cross_thickness, size * 1.15f }, style.colour, -math::PI<float> * 0.25f);
            break;
        case PieceShape::Square:
            Renderer::draw_rect(centre, { size * k_outline, size * k_outline }, theme.piece_outline);
            Renderer::draw_rect(centre, { size, size }, style.colour);
            break;
    }
}

void draw_space(const BoardLayout2D& board, SpaceId space, SpaceHighlight highlight, const BoardProjection2D& layout, const BoardTheme2D& theme)
{
    Vec2f centre = board_to_world(layout, board.position(space));
    Vec2f size = { board.size(space)[0] * layout.scale * (1.0f - theme.space_gap), board.size(space)[1] * layout.scale * (1.0f - theme.space_gap) };
    float radius = std::min(size[0], size[1]) * 0.5f;

    auto fill = [&](const Colour& colour)
    {
        if (board.shape(space) == SpaceShape::Circle)
        {
            Renderer::draw_circle(centre, radius, colour);
        }
        else
        {
            Renderer::draw_rect(centre, size, colour);
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

void draw_axis_labels(const BoardLayout2D& board, const BoardProjection2D& layout, const BoardTheme2D& theme, Font& font)
{
    const std::vector<float>& columns = board.columns();
    const std::vector<float>& rows = board.rows();
    if (board.column_labels().size() == columns.size())
    {
        float y = board_to_world(layout, { 0.0f, board.min()[1] })[1] - k_board_gutter * 0.5f;
        for (size_t index = 0; index < columns.size(); ++index)
        {
            draw_text_centred(font, { board_to_world(layout, { columns[index], 0.0f })[0], y }, board.column_labels()[index], k_label_height, theme.label);
        }
    }
    if (board.row_labels().size() == rows.size())
    {
        float x = board_to_world(layout, { board.min()[0], 0.0f })[0] - k_board_gutter * 0.5f;
        for (size_t index = 0; index < rows.size(); ++index)
        {
            draw_text_centred(font, { x, board_to_world(layout, { 0.0f, rows[index] })[1] }, board.row_labels()[rows.size() - 1 - index], k_label_height, theme.label);
        }
    }
}

} // namespace

void draw_board_2d(const BoardScene& scene, const BoardProjection2D& layout, const BoardTheme2D& theme, Font& font, const std::string& status)
{
    if (layout.scale <= 0.0f || scene.layout == nullptr)
    {
        return;
    }
    const BoardLayout2D& board = layout_as<BoardLayout2D>(scene.layout, "board");

    for (SpaceId space = 0; space < board.space_count(); ++space)
    {
        draw_space(board, space, scene.highlights[space], layout, theme);
    }

    for (const ScenePiece& piece : scene.pieces)
    {
        if (piece.space >= board.space_count())
        {
            continue;
        }
        bool dragged = scene.drag.active && scene.drag.space == piece.space;
        Vec2f centre = dragged ? cursor_to_world(layout, scene.drag.cursor) : board_to_world(layout, board.position(piece.space));
        draw_piece(piece.style, centre, std::min(board.size(piece.space)[0], board.size(piece.space)[1]) * layout.scale * theme.piece_size, theme);
    }

    for (SpaceId space = 0; space < board.space_count(); ++space)
    {
        if (has_highlight(scene.highlights[space], SpaceHighlight::Target))
        {
            Vec2f centre = board_to_world(layout, board.position(space));
            Renderer::draw_circle(centre, std::min(board.size(space)[0], board.size(space)[1]) * layout.scale * k_target_dot, theme.target);
        }
    }

    draw_axis_labels(board, layout, theme, font);

    for (size_t index = 0; index < scene.options.size(); ++index)
    {
        OptionButton2D button = option_button_2d(layout, scene.options.size(), index);
        Renderer::draw_rect(button.centre, button.size, theme.button);
        draw_text_centred(font, button.centre, scene.options[index].label, k_option_height, theme.status);
    }

    draw_text_centred(font, { layout.viewport[0] * 0.5f, layout.viewport[1] - k_board_status_band * 0.5f }, status.empty() ? scene.status : status, k_status_height, theme.status);
}

} // namespace oryx
