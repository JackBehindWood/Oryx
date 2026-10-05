#include "BoardRenderer2D.h"

#include "Oryx/Renderer/Renderer.h"

namespace oryx
{

namespace
{

constexpr float kLabelHeight = 16.0f;
constexpr float kStatusHeight = 24.0f;
constexpr float kOptionHeight = 18.0f;
constexpr float kTargetDot = 0.16f;
constexpr float kRingThickness = 0.22f;
constexpr float kCrossThickness = 0.16f;
constexpr float kOutline = 1.08f;

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
            Renderer::draw_circle(centre, radius * kOutline, theme.piece_outline);
            Renderer::draw_circle(centre, radius, style.colour);
            break;
        case PieceShape::Ring:
            Renderer::draw_circle(centre, radius, style.colour, kRingThickness);
            break;
        case PieceShape::Cross:
            Renderer::draw_rect(centre, { size * kCrossThickness, size * 1.15f }, style.colour, math::PI<float> * 0.25f);
            Renderer::draw_rect(centre, { size * kCrossThickness, size * 1.15f }, style.colour, -math::PI<float> * 0.25f);
            break;
        case PieceShape::Square:
            Renderer::draw_rect(centre, { size * kOutline, size * kOutline }, theme.piece_outline);
            Renderer::draw_rect(centre, { size, size }, style.colour);
            break;
    }
}

void draw_space(const SceneSpace& scene_space, const BoardLayout2D& layout, const BoardTheme2D& theme)
{
    const BoardSpace& space = scene_space.space;
    Vec2f centre = board_to_world(layout, { space.position[0], space.position[1] });
    Vec2f size = { space.size[0] * layout.scale * (1.0f - theme.space_gap), space.size[1] * layout.scale * (1.0f - theme.space_gap) };
    float radius = std::min(size[0], size[1]) * 0.5f;

    auto fill = [&](const Colour& colour)
    {
        if (space.shape == SpaceShape::Circle)
        {
            Renderer::draw_circle(centre, radius, colour);
        }
        else
        {
            Renderer::draw_rect(centre, size, colour);
        }
    };

    fill(theme.tones[space.tone % 2]);
    if ((scene_space.highlight & kHighlightChanged) != 0)
    {
        fill(theme.changed);
    }
    if ((scene_space.highlight & kHighlightPicked) != 0)
    {
        fill(theme.picked);
    }
    if ((scene_space.highlight & kHighlightHover) != 0)
    {
        fill(theme.hover);
    }
}

void draw_axis_labels(const BoardScene& scene, const BoardLayout2D& layout, const BoardTheme2D& theme, Font& font)
{
    std::vector<float> columns = scene_columns(scene);
    std::vector<float> rows = scene_rows(scene);
    if (scene.column_labels.size() == columns.size())
    {
        float y = board_to_world(layout, { 0.0f, scene.min[1] })[1] - kBoardGutter * 0.5f;
        for (size_t index = 0; index < columns.size(); ++index)
        {
            draw_text_centred(font, { board_to_world(layout, { columns[index], 0.0f })[0], y }, scene.column_labels[index], kLabelHeight, theme.label);
        }
    }
    if (scene.row_labels.size() == rows.size())
    {
        float x = board_to_world(layout, { scene.min[0], 0.0f })[0] - kBoardGutter * 0.5f;
        for (size_t index = 0; index < rows.size(); ++index)
        {
            draw_text_centred(font, { x, board_to_world(layout, { 0.0f, rows[index] })[1] }, scene.row_labels[rows.size() - 1 - index], kLabelHeight, theme.label);
        }
    }
}

} // namespace

void draw_board_2d(const BoardScene& scene, const BoardLayout2D& layout, const BoardTheme2D& theme, Font& font, const std::string& status)
{
    if (layout.scale <= 0.0f)
    {
        return;
    }

    for (const SceneSpace& space : scene.spaces)
    {
        draw_space(space, layout, theme);
    }

    for (const ScenePiece& piece : scene.pieces)
    {
        if (piece.space >= scene.spaces.size())
        {
            continue;
        }
        const BoardSpace& space = scene.spaces[piece.space].space;
        Vec2f centre = board_to_world(layout, { space.position[0], space.position[1] });
        draw_piece(piece.style, centre, std::min(space.size[0], space.size[1]) * layout.scale * theme.piece_size, theme);
    }

    for (const SceneSpace& space : scene.spaces)
    {
        if ((space.highlight & kHighlightTarget) != 0)
        {
            Vec2f centre = board_to_world(layout, { space.space.position[0], space.space.position[1] });
            Renderer::draw_circle(centre, std::min(space.space.size[0], space.space.size[1]) * layout.scale * kTargetDot, theme.target);
        }
    }

    draw_axis_labels(scene, layout, theme, font);

    std::vector<OptionButton2D> buttons;
    option_buttons_2d(layout, scene.options.size(), buttons);
    for (size_t index = 0; index < buttons.size(); ++index)
    {
        Renderer::draw_rect(buttons[index].centre, buttons[index].size, theme.button);
        draw_text_centred(font, buttons[index].centre, scene.options[index].label, kOptionHeight, theme.status);
    }

    draw_text_centred(font, { layout.viewport[0] * 0.5f, layout.viewport[1] - kBoardStatusBand * 0.5f }, status.empty() ? scene.status : status, kStatusHeight, theme.status);
}

} // namespace oryx
