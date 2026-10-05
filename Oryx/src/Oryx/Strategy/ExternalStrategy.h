#pragma once

#include "Oryx/Strategy/IActionSource.h"

namespace oryx
{

class ExternalStrategy : public IStrategy
{
public:
    explicit ExternalStrategy(SharedPtr<IActionSource> source)
        : m_source(std::move(source))
    {
    }

    ActionId decide(const Context& context) override { return m_source->next_action(context); }

private:
    SharedPtr<IActionSource> m_source;
};

} // namespace oryx
