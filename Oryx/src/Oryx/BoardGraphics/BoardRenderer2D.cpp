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

void draw_space(const SceneSpace& scene_space, const BoardProjection2D& layout, const BoardTheme2D& theme)
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
    if (has_highlight(scene_space.highlight, SpaceHighlight::Changed))
    {
        fill(theme.changed);
    }
    if (has_highlight(scene_space.highlight, SpaceHighlight::Picked))
    {
        fill(theme.picked);
    }
    if (has_highlight(scene_space.highlight, SpaceHighlight::Hover))
    {
        fill(theme.hover);
    }
}

void draw_axis_labels(const BoardScene& scene, const BoardProjection2D& layout, const BoardTheme2D& theme, Font& font)
{
    std::vector<float> columns = scene_columns(scene);
    std::vector<float> rows = scene_rows(scene);
    if (scene.column_labels.size() == columns.size())
    {
        float y = board_to_world(layout, { 0.0f, scene.min[1] })[1] - k_board_gutter * 0.5f;
        for (size_t index = 0; index < columns.size(); ++index)
        {
            draw_text_centred(font, { board_to_world(layout, { columns[index], 0.0f })[0], y }, scene.column_labels[index], k_label_height, theme.label);
        }
    }
    if (scene.row_labels.size() == rows.size())
    {
        float x = board_to_world(layout, { scene.min[0], 0.0f })[0] - k_board_gutter * 0.5f;
        for (size_t index = 0; index < rows.size(); ++index)
        {
            draw_text_centred(font, { x, board_to_world(layout, { 0.0f, rows[index] })[1] }, scene.row_labels[rows.size() - 1 - index], k_label_height, theme.label);
        }
    }
}

} // namespace

void draw_board_2d(const BoardScene& scene, const BoardProjection2D& layout, const BoardTheme2D& theme, Font& font, const std::string& status)
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
        if (has_highlight(space.highlight, SpaceHighlight::Target))
        {
            Vec2f centre = board_to_world(layout, { space.space.position[0], space.space.position[1] });
            Renderer::draw_circle(centre, std::min(space.space.size[0], space.space.size[1]) * layout.scale * k_target_dot, theme.target);
        }
    }

    draw_axis_labels(scene, layout, theme, font);

    std::vector<OptionButton2D> buttons;
    option_buttons_2d(layout, scene.options.size(), buttons);
    for (size_t index = 0; index < buttons.size(); ++index)
    {
        Renderer::draw_rect(buttons[index].centre, buttons[index].size, theme.button);
        draw_text_centred(font, buttons[index].centre, scene.options[index].label, k_option_height, theme.status);
    }

    draw_text_centred(font, { layout.viewport[0] * 0.5f, layout.viewport[1] - k_board_status_band * 0.5f }, status.empty() ? scene.status : status, k_status_height, theme.status);
}

} // namespace oryx
