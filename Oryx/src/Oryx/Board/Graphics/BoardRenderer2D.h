#pragma once

#include "Oryx/Board/Layout/BoardLayout2D.h"
#include "Oryx/Board/Graphics/BoardProjection2D.h"
#include "Oryx/Renderer/Font.h"

namespace oryx
{

struct BoardTheme2D
{
    Colour tones[2] = { { 0.22f, 0.24f, 0.30f, 1.0f }, { 0.15f, 0.16f, 0.21f, 1.0f } };
    Colour changed = { 0.95f, 0.80f, 0.30f, 0.22f };
    Colour picked = { 0.95f, 0.80f, 0.30f, 0.50f };
    Colour hover = { 1.0f, 1.0f, 1.0f, 0.10f };
    Colour target = { 0.55f, 0.85f, 0.55f, 0.55f };
    Colour piece_outline = { 0.05f, 0.05f, 0.07f, 1.0f };
    Colour label = { 0.60f, 0.62f, 0.70f, 1.0f };
    Colour status = { 1.0f, 1.0f, 1.0f, 1.0f };
    Colour button = { 0.25f, 0.28f, 0.36f, 1.0f };
    // Fraction of a space's footprint left empty around its tile, so a grid reads as cells.
    float space_gap = 0.06f;
    // A piece's size as a fraction of its space's footprint.
    float piece_size = 0.64f;
};

// Draws a scene through the Renderer facade; the caller opens and closes the scene with a pixel-unit Camera2D over layout.viewport.
// `status` replaces scene.status when not empty (e.g. with a restart hint).
void draw_board_2d(const BoardScene& scene, const BoardProjection2D& layout, const BoardTheme2D& theme, Font& font, const std::string& status = {});

} // namespace oryx
