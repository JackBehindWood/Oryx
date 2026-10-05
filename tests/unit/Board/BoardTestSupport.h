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

    int32_t turns = 0;
    ActionId next_action = PENDING_ACTION;
};

class FakeGraphicsBoard : public IGraphicsBoard
{
public:
    void on_turn(const IState&) override {}
    ActionId poll_action(const IState&) override { return PENDING_ACTION; }
    void update(const BoardInput&, double) override {}
    void render(const BoardInput&) override {}
    bool shows_moves() const override { return true; }
};

// Shows DummyGame as a single pile whose moves are menu options ("take1".."take3"), and remembers whose view it last described.
class FakePresenter : public IBoardPresenter
{
public:
    void describe(const IState& state, PlayerId viewer, BoardView& out) const override
    {
        last_viewer = viewer;
        out.spaces.push_back({ { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, SpaceShape::Square, 0, "pile" });
        out.status = state.is_terminal() ? "over" : "player " + std::to_string(state.current_player() + 1);
    }

    void action_picks(const IState&, ActionId action, PickList& out) const override
    {
        out.push_back({ PickKind::Option, action, "take" + std::to_string(action) });
    }

    PieceStyle piece_style(PieceKind, PlayerId) const override { return { "o", {}, PieceShape::Disc }; }

    mutable PlayerId last_viewer = -2;
};

inline UniquePtr<IState> finished_dummy_state()
{
    UniquePtr<IState> state = DummyGame(1).new_initial_state();
    state->apply(1);
    return state;
}

} // namespace oryx::test
