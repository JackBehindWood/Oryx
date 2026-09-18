#pragma once

#include "Oryx.h"

#include "Oasis/Game/TicTacToeBoard.h"

namespace oasis
{

using oryx::ActionId;
using oryx::Context;
using oryx::IGame;
using oryx::IState;
using oryx::UniquePtr;

class OasisLayer : public oryx::Layer
{
public:
    explicit OasisLayer(std::string opponent_arg = "");

    void attach() override;
    void update() override;
    void detach() override;

private:
    bool prompt_for_opponent(std::string& out_name) const;

    std::string m_opponent_arg;
    UniquePtr<IGame> m_game;
    UniquePtr<IState> m_state;
    TicTacToeBoard m_board;
    std::vector<ActionId> m_history;

    UniquePtr<oryx::IStrategy> m_opponent;
    int32_t m_human_player = 0;
};

} // namespace oasis
