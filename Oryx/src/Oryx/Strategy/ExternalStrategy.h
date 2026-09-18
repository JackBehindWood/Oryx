#pragma once

#include "Oryx/Strategy/IStrategy.h"

namespace oryx
{


class ExternalStrategy : public IStrategy
{
public:
    using InputProvider = std::function<ActionId(const Context&)>;

    explicit ExternalStrategy(InputProvider provider) : m_provider(std::move(provider)) {}

    ActionId decide(const Context& context) override { return m_provider(context); }

private:
    InputProvider m_provider;
};

} // namespace oryx
