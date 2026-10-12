#include "PresenterActionDescriber.h"

namespace oryx
{

PresenterActionDescriber::PresenterActionDescriber(SharedPtr<const IBoardPresenter> presenter)
    : m_presenter(std::move(presenter))
{
    m_picks.reserve(k_record_spaces);
    m_text.reserve(k_label_capacity * 2);
}

bool PresenterActionDescriber::describe(const IState& state, ActionId action, ActionDescription& out) const
{
    SharedPtr<const BoardLayout> layout = m_presenter->layout(state);
    if (layout == nullptr)
    {
        return false;
    }
    m_picks.clear();
    m_presenter->action_picks(state, action, m_picks);
    m_text.clear();
    out.space_count = 0;
    for (const Pick& pick : m_picks)
    {
        if (pick.kind == PickKind::Space && pick.value < layout->space_count())
        {
            m_text += m_text.empty() ? "" : "-";
            m_text += layout->label(pick.value);
            if (out.space_count < k_record_spaces)
            {
                out.spaces[out.space_count++] = pick.value;
            }
        }
        else if (pick.kind == PickKind::Option && !pick.label.empty())
        {
            m_text += '=';
            m_text += pick.label;
        }
    }
    if (out.space_count == 0)
    {
        return false;
    }
    set_label(out.label, m_text);
    return true;
}

SharedPtr<const IActionDescriber> make_presenter_describer(const std::string& game)
{
    UniquePtr<IBoardPresenter> presenter = BoardPresenterRegistry::create(game);
    if (presenter == nullptr)
    {
        return nullptr;
    }
    return create_shared<PresenterActionDescriber>(SharedPtr<const IBoardPresenter>(std::move(presenter)));
}

} // namespace oryx
