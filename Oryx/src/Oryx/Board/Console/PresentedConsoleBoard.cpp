#include "PresentedConsoleBoard.h"

#include <iostream>

namespace oryx
{

namespace
{

size_t display_width(const std::string& text)
{
    return static_cast<size_t>(std::count_if(text.begin(), text.end(), [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }));
}

std::string centred(const std::string& text, size_t width)
{
    size_t length = display_width(text);
    if (length >= width)
    {
        return text;
    }
    size_t left = (width - length) / 2;
    return std::string(left, ' ') + text + std::string(width - length - left, ' ');
}

void trim_right(std::string& line)
{
    line.erase(line.find_last_not_of(' ') + 1);
}

std::string pick_name(const BoardLayout& layout, const Pick& pick)
{
    if (pick.kind == PickKind::Space && pick.value < layout.space_count())
    {
        return layout.label(pick.value);
    }
    return pick.label;
}

bool same_word(const std::string& a, const std::string& b)
{
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y)); });
}

} // namespace

std::string board_text(const BoardScene& scene)
{
    if (scene.layout == nullptr)
    {
        return scene.status.empty() ? std::string() : scene.status + "\n";
    }
    const BoardLayout& layout = *scene.layout;
    size_t count = layout.space_count();

    std::vector<std::string> glyphs(count, ".");
    for (const ScenePiece& piece : scene.pieces)
    {
        if (piece.space < glyphs.size() && !piece.style.glyph.empty())
        {
            glyphs[piece.space] = piece.style.glyph;
        }
    }

    std::string text;
    if (!layout.text_grid())
    {
        for (SpaceId space = 0; space < count; ++space)
        {
            text += layout.label(space) + ": " + glyphs[space] + "\n";
        }
        if (!scene.status.empty())
        {
            text += scene.status + "\n";
        }
        return text;
    }

    const std::vector<float>& xs = layout.columns();
    const std::vector<float>& ys = layout.rows();

    size_t glyph_width = 1;
    for (const std::string& glyph : glyphs)
    {
        glyph_width = std::max(glyph_width, display_width(glyph));
    }
    size_t cell_width = glyph_width + 2;

    std::vector<std::string> cells(xs.size() * ys.size(), std::string(cell_width, ' '));
    for (SpaceId space = 0; space < count; ++space)
    {
        SpaceHighlight highlight = scene.highlights[space];
        std::string open = " ";
        std::string close = " ";
        if (has_highlight(highlight, SpaceHighlight::Picked))
        {
            open = "(";
            close = ")";
        }
        else if (has_highlight(highlight, SpaceHighlight::Changed))
        {
            open = "[";
            close = "]";
        }
        cells[layout.row_index(space) * xs.size() + layout.column_index(space)] = open + centred(glyphs[space], glyph_width) + close;
    }

    bool row_labels = layout.row_labels().size() == ys.size();
    bool column_labels = layout.column_labels().size() == xs.size();
    size_t label_width = 0;
    if (row_labels)
    {
        for (const std::string& label : layout.row_labels())
        {
            label_width = std::max(label_width, display_width(label));
        }
    }
    std::string margin = row_labels ? std::string(label_width + 1, ' ') : "";

    for (size_t row = 0; row < ys.size(); ++row)
    {
        std::string line;
        if (row_labels)
        {
            const std::string& label = layout.row_labels()[ys.size() - 1 - row];
            line += std::string(label_width - display_width(label), ' ') + label + " ";
        }
        for (size_t col = 0; col < xs.size(); ++col)
        {
            line += cells[row * xs.size() + col];
        }
        trim_right(line);
        text += line + "\n";
    }

    if (column_labels)
    {
        std::string line = margin;
        for (const std::string& label : layout.column_labels())
        {
            line += centred(label, cell_width);
        }
        trim_right(line);
        text += line + "\n";
    }

    if (!scene.status.empty())
    {
        text += scene.status + "\n";
    }
    return text;
}

PresentedConsoleBoard::PresentedConsoleBoard(UniquePtr<IBoardPresenter> presenter, std::string game, PlayerId seat)
    : m_interaction(std::move(presenter), std::move(game), seat)
{
}

void PresentedConsoleBoard::on_turn(const IState& state)
{
    if (m_interaction.update(state))
    {
        print();
    }
}

void PresentedConsoleBoard::print()
{
    std::cout << board_text(m_interaction.scene());
}

ActionId PresentedConsoleBoard::poll_action(const IState& state)
{
    on_turn(state);
    if (!m_interaction.accepts_moves())
    {
        return PENDING_ACTION;
    }

    const MoveBuilder& builder = m_interaction.presentation().builder();
    const BoardLayout& layout = *m_interaction.presentation().view().layout;
    while (std::cin)
    {
        std::string moves;
        for (const Pick& pick : builder.next_picks())
        {
            moves += (moves.empty() ? "" : " ") + pick_name(layout, pick);
        }
        std::cout << (builder.picked().empty() ? "Moves: " : "Then: ") << moves << "\n";
        std::cout << "Enter a move, 'b' to take back a pick or 'u' to undo: ";

        std::string line;
        if (!std::getline(std::cin, line))
        {
            break;
        }

        std::istringstream words(line);
        std::string word;
        ActionId action = PENDING_ACTION;
        bool understood = true;
        while (understood && action == PENDING_ACTION && words >> word)
        {
            if (word == "u" || word == "undo")
            {
                m_interaction.undo();
                return m_interaction.poll();
            }
            if (word == "b" || word == "back")
            {
                m_interaction.back();
                continue;
            }

            PickList next = builder.next_picks();
            PickList::iterator match = std::find_if(next.begin(), next.end(), [&](const Pick& pick) { return same_word(pick_name(layout, pick), word); });
            understood = match != next.end();
            if (understood)
            {
                m_interaction.submit(*match);
                action = m_interaction.poll();
            }
        }

        if (!understood)
        {
            m_interaction.cancel();
            std::cout << "That isn't a legal move. Try again.\n";
            continue;
        }
        if (action != PENDING_ACTION)
        {
            return action;
        }
    }
    return PENDING_ACTION;
}

} // namespace oryx
