#pragma once

#include "unit/Game/DummyGame.h"

namespace oryx::test
{

// Reads std::cin from a string for the lifetime of the object and captures std::cout.
class ConsoleScope
{
public:
    explicit ConsoleScope(const std::string& input)
        : m_input(input)
        , m_in_before(std::cin.rdbuf(m_input.rdbuf()))
        , m_out_before(std::cout.rdbuf(m_output.rdbuf()))
    {
        std::cin.clear();
    }

    ~ConsoleScope()
    {
        std::cin.rdbuf(m_in_before);
        std::cout.rdbuf(m_out_before);
        std::cin.clear();
    }

    ConsoleScope(const ConsoleScope&) = delete;
    ConsoleScope& operator=(const ConsoleScope&) = delete;

    [[nodiscard]] std::string output() const { return m_output.str(); }

private:
    std::istringstream m_input;
    std::ostringstream m_output;
    std::streambuf* m_in_before;
    std::streambuf* m_out_before;
};

class FakeBoard : public IBoard
{
public:
    void on_turn(const IState&) override { ++turns; }
    ActionId poll_action(const IState&) override { return next_action; }
    bool shows_moves() const override { return true; }
    void reset(PlayerId seat) override
    {
        ++resets;
        last_seat = seat;
    }

    int32_t turns = 0;
    int32_t resets = 0;
    PlayerId last_seat = -2;
    ActionId next_action = PENDING_ACTION;
};

// Shows DummyGame as a single pile whose moves are menu options ("take1".."take3"), and remembers whose view it last described.
class FakePresenter : public IBoardPresenter
{
public:
    FakePresenter()
    {
        BoardLayout2D builder;
        builder.add_space("pile", { 0.0f, 0.0f }, { 1.0f, 1.0f }, SpaceShape::Square);
        m_layout = builder.finish();
    }

    SharedPtr<const BoardLayout> layout(const IState&) const override { return m_layout; }

    void describe_pieces(const IState& state, PlayerId viewer, BoardContent& out) const override
    {
        last_viewer = viewer;
        out.status = state.is_terminal() ? "over" : "player " + std::to_string(state.current_player() + 1);
    }

    void action_picks(const IState&, ActionId action, PickList& out) const override
    {
        out.push_back({ PickKind::Option, action, "take" + std::to_string(action) });
    }

    PieceStyle piece_style(PieceKind, PlayerId) const override { return { "o", {}, PieceShape::Disc }; }

    mutable PlayerId last_viewer = -2;

private:
    SharedPtr<const BoardLayout> m_layout;
};

// A non-grid board: nine circular points on a ring, each a space of the DummyGame pile's moves.
inline SharedPtr<const BoardLayout2D> make_ring_layout()
{
    BoardLayout2D builder;
    for (uint32_t index = 0; index < 9; ++index)
    {
        float angle = static_cast<float>(index) * 2.0f * math::PI<float> / 9.0f;
        builder.add_space("p" + std::to_string(index), { 3.0f * std::cos(angle), 3.0f * std::sin(angle) }, { 0.8f, 0.8f }, SpaceShape::Circle);
    }
    return builder.finish();
}

class RingPresenter : public FakePresenter
{
public:
    RingPresenter()
        : m_ring(make_ring_layout())
    {
    }

    SharedPtr<const BoardLayout> layout(const IState&) const override { return m_ring; }

    void describe_pieces(const IState& state, PlayerId viewer, BoardContent& out) const override
    {
        FakePresenter::describe_pieces(state, viewer, out);
        out.pieces.push_back({ 0, 0, 4 });
    }

private:
    SharedPtr<const BoardLayout2D> m_ring;
};

// A 19x19 checkered board with a few pieces, for steady-state cost checks.
class Grid19Presenter : public FakePresenter
{
public:
    Grid19Presenter()
        : m_grid(make_grid_layout(19, 19, true))
    {
    }

    SharedPtr<const BoardLayout> layout(const IState&) const override { return m_grid; }

    void describe_pieces(const IState& state, PlayerId viewer, BoardContent& out) const override
    {
        FakePresenter::describe_pieces(state, viewer, out);
        for (SpaceId space = 0; space < 40; space += 3)
        {
            out.pieces.push_back({ 0, static_cast<PlayerId>(space % 2), space });
        }
    }

private:
    SharedPtr<const BoardLayout2D> m_grid;
};

inline UniquePtr<IState> finished_dummy_state()
{
    UniquePtr<IState> state = DummyGame(1).new_initial_state();
    state->apply(1);
    return state;
}

} // namespace oryx::test
