#pragma once

#include "Oryx/Board/IBoardPresenter.h"
#include "Oryx/Dashboard/Feed/ActionDescription.h"

namespace oryx
{

// Names an action by its board picks ("a3", "a3-b2", "a1-c3-e5"), so the dashboard and the board speak the same vocabulary.
class PresenterActionDescriber final : public IActionDescriber
{
public:
    explicit PresenterActionDescriber(SharedPtr<const IBoardPresenter> presenter);

    bool describe(const IState& state, ActionId action, ActionDescription& out) const override;

private:
    SharedPtr<const IBoardPresenter> m_presenter;
    mutable PickList m_picks;
    mutable std::string m_text;
};

// Null when no presenter is registered for `game`.
[[nodiscard]] SharedPtr<const IActionDescriber> make_presenter_describer(const std::string& game);

} // namespace oryx
