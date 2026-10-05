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
    void render(double) override {}
    bool shows_moves() const override { return true; }
};

inline UniquePtr<IState> finished_dummy_state()
{
    UniquePtr<IState> state = DummyGame(1).new_initial_state();
    state->apply(1);
    return state;
}

} // namespace oryx::test
