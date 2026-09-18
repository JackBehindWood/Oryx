#pragma once

#include "Oryx.h"

#include "Oasis/Game/TicTacToeBoard.h"

namespace oasis
{

class OasisLayer : public oryx::Layer
{
public:
    explicit OasisLayer(std::string opponent_arg = "", std::string simulate_arg = "", bool benchmark_arg = false);

    void attach() override;
    void event(oryx::Event& event) override;

private:
    bool prompt_for_opponent(std::string& out_name) const;
    void attach_simulate(oryx::UniquePtr<oryx::IGame> game);
    void attach_interactive(oryx::UniquePtr<oryx::IGame> game);
    bool on_simulation_complete(const oryx::SimulationCompleteEvent& event);

    std::string m_opponent_arg;
    std::string m_simulate_arg;
    bool m_benchmark_arg;
    TicTacToeBoard m_board;
};

} // namespace oasis
