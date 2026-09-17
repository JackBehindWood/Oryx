#pragma once

#include "Oryx.h"

namespace oasis
{

class OasisLayer : public oryx::Layer
{
public:
    OasisLayer();

    void attach() override;
};

} // namespace oasis
