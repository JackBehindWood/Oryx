#include "BoardInteraction.h"

namespace oryx
{

BoardInteraction::BoardInteraction(UniquePtr<IBoardPresenter> presenter, std::string game, PlayerId seat)
    : m_presentation(std::move(presenter), std::move(game), seat)
{
}

void BoardInteraction::reset(PlayerId seat)
{
    m_presentation.reset(seat);
    m_scene = {};
    m_queued = PENDING_ACTION;
    m_hovered = k_no_space;
    m_drag_space = k_no_space;
    m_drag = false;
}

bool BoardInteraction::update(const IState& state)
{
    if (!m_presentation.update(state))
    {
        return false;
    }
    m_queued = PENDING_ACTION;
    m_drag = false;
    return true;
}

PickStatus BoardInteraction::take(const Pick& pick)
{
    MoveBuilder& builder = m_presentation.builder();
    PickResult result = builder.pick(pick);
    if (result.status == PickStatus::Rejected)
    {
        builder.clear();
        m_drag = false;
    }
    else if (result.status == PickStatus::Complete)
    {
        m_queued = result.action;
        m_drag = false;
    }
    return result.status;
}

void BoardInteraction::press(SpaceId space)
{
    if (!accepts_moves())
    {
        return;
    }
    if (space == k_no_space)
    {
        cancel();
        return;
    }
    PickStatus status = take(space_pick(space));
    if (status == PickStatus::Pending || status == PickStatus::Ready)
    {
        m_drag = true;
        m_drag_space = space;
    }
}

void BoardInteraction::release(SpaceId space)
{
    if (!m_drag)
    {
        return;
    }
    SpaceId from = m_drag_space;
    m_drag = false;
    if (!accepts_moves() || space == k_no_space || space == from)
    {
        return;
    }
    if (m_presentation.builder().can_pick(space_pick(space)))
    {
        take(space_pick(space));
    }
}

void BoardInteraction::choose_option(size_t index)
{
    if (!accepts_moves())
    {
        return;
    }
    const PickList& options = scene().options;
    if (index < options.size())
    {
        take(options[index]);
    }
}

void BoardInteraction::confirm()
{
    if (m_presentation.builder().ready())
    {
        submit({ PickKind::Confirm, 0, {} });
    }
}

void BoardInteraction::back()
{
    m_drag = false;
    if (accepts_moves())
    {
        m_presentation.builder().back();
    }
}

void BoardInteraction::undo()
{
    if (!accepts_moves())
    {
        return;
    }
    cancel();
    m_queued = UNDO_ACTION;
}

void BoardInteraction::cancel()
{
    m_drag = false;
    m_presentation.builder().clear();
}

void BoardInteraction::submit(const Pick& pick)
{
    if (accepts_moves())
    {
        take(pick);
    }
}

ActionId BoardInteraction::poll()
{
    ActionId action = m_queued;
    m_queued = PENDING_ACTION;
    return action;
}

const BoardScene& BoardInteraction::scene()
{
    m_presentation.build_scene(m_hovered, m_scene, { m_drag, m_drag_space, m_cursor });
    return m_scene;
}

} // namespace oryx
