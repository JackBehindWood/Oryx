#pragma once

#include "Oryx.h"

#include "Oasis/Game/TicTacToeBoard.h"

namespace oasis
{

using oryx::ActionId;
using oryx::IGame;
using oryx::IState;
using oryx::UniquePtr;

class OasisLayer : public oryx::Layer
{
public:
    OasisLayer();

    void attach() override;
    void update() override;
    void detach() override;

private:
    UniquePtr<IGame> m_game;
    UniquePtr<IState> m_state;
    TicTacToeBoard m_board;
    std::vector<ActionId> m_history;
};

} // namespace oasis
