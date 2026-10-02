#include "oxpch.h"
#include "Oryx/Core/Window.h"

#if defined(OX_ENABLE_GRAPHICS) && defined(OX_PLATFORM_MACOS)
#include "MacOSWindow.h"
#else
#include "NullWindow.h"
#endif

namespace oryx
{

UniquePtr<Window> Window::create(WindowDesc desc)
{
#if defined(OX_ENABLE_GRAPHICS) && defined(OX_PLATFORM_MACOS)
    return create_unique<MacOSWindow>(std::move(desc));
#else
    return create_unique<NullWindow>(std::move(desc));
#endif
}

} // namespace oryx
