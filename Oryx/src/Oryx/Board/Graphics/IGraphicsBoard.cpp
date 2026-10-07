#include "IGraphicsBoard.h"

#include "Oryx/Core/Application.h"

namespace oryx
{

void IGraphicsBoard::frame(const FrameInfo& info)
{
    BoardInput input = read_board_input(info.input, info.logical, info.scale);
    if (input.quit)
    {
        Application::Get().close();
        return;
    }
    update(input, info.delta_time);
    render(input);
    m_restart_requested = m_restart_requested || input.restart;
}

bool IGraphicsBoard::take_restart_request()
{
    bool requested = m_restart_requested;
    m_restart_requested = false;
    return requested;
}

} // namespace oryx
