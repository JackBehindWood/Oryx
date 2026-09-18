#pragma once

#include "Oryx/Strategy/IStrategy.h"

namespace oryx
{

class FirstLegalStrategy : public IStrategy
{
public:
    ActionId decide(const Context& context) override;
};

} // namespace oryx
