#pragma once

#include "Oryx/Board/Layout/BoardLayout2D.h"
#include "Oryx/Interface/UI/UiTheme.h"
#include "Oryx/Board/Graphics/BoardProjection2D.h"
#include "Oryx/Renderer/Batch/BatchRenderer2D.h"
#include "Oryx/Renderer/Font.h"

namespace oryx
{

// The overlay's flat look: square buttons without an outline, plain white status text.
[[nodiscard]] UiTheme board_ui_theme();

struct BoardTheme2D
{
    Colour tones[2] = { { 0.22f, 0.24f, 0.30f, 1.0f }, { 0.15f, 0.16f, 0.21f, 1.0f } };
    Colour changed = { 0.95f, 0.80f, 0.30f, 0.22f };
    Colour picked = { 0.95f, 0.80f, 0.30f, 0.50f };
    Colour hover = { 1.0f, 1.0f, 1.0f, 0.10f };
    Colour target = { 0.55f, 0.85f, 0.55f, 0.55f };
    Colour piece_outline = { 0.05f, 0.05f, 0.07f, 1.0f };
    Colour label = { 0.60f, 0.62f, 0.70f, 1.0f };
    // The option buttons and the status line; its font is replaced by `font` below.
    UiTheme ui = board_ui_theme();
    // Fraction of a space's footprint left empty around its tile, so a grid reads as cells.
    float space_gap = 0.06f;
    // A piece's size as a fraction of its space's footprint.
    float piece_size = 0.64f;
    // Null draws with Renderer::default_font().
    Font* font = nullptr;
};

// The board proper (spaces, pieces, targets, axis labels) through `batcher`, whose scene the caller has open under a pixel-unit Camera2D over layout.viewport.
void draw_board_2d(BatchRenderer2D& batcher, const BoardScene& scene, const BoardProjection2D& layout, const BoardTheme2D& theme, Font& font);

} // namespace oryx
