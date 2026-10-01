#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Core/KeyCode.h"
#include "Oryx/Core/MouseCode.h"

namespace oryx
{

class IInput
{
public:
    virtual ~IInput() = default;

    [[nodiscard]] virtual bool key_down(KeyCode key) const = 0;
    [[nodiscard]] virtual bool mouse_down(MouseCode button) const = 0;
    [[nodiscard]] virtual float cursor_x() const = 0;
    [[nodiscard]] virtual float cursor_y() const = 0;
};

} // namespace oryx
