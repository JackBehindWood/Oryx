#pragma once

#include "Oryx.h"

#include "Oasis/Game/TicTacToeBoard.h"

namespace oasis
{

// Oasis-specific setup only: resolves/prompts for the game and opponent,
// builds the ExternalStrategy (human input) and render hooks, and pushes
// SimulationLayer, which owns the actual game/action loop
// (Oryx/Simulation/SimulationLayer.h).
class OasisLayer : public oryx::Layer
{
public:
    explicit OasisLayer(std::string opponent_arg = "", std::string simulate_arg = "");

    void attach() override;

private:
    bool prompt_for_opponent(std::string& out_name) const;
    void attach_simulate(oryx::UniquePtr<oryx::IGame> game);
    void attach_interactive(oryx::UniquePtr<oryx::IGame> game);

    std::string m_opponent_arg;
    std::string m_simulate_arg;
    TicTacToeBoard m_board;
};

} // namespace oasis
