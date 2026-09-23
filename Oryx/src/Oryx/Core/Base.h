#pragma once

#include "Oryx/Core/PlatformDetection.h"

#include <memory>

#include "Oryx/Memory/SmartPointers.h"

#ifdef OX_DEBUG
	#if defined(OX_PLATFORM_WINDOWS)
		#define OX_DEBUGBREAK() __debugbreak()
	#elif defined(OX_PLATFORM_LINUX) || defined(OX_PLATFORM_MACOS)
		#include <signal.h>
		#define OX_DEBUGBREAK() raise(SIGTRAP)
	#else
		#error "Platform doesn't support debugbreak yet!"
	#endif
	#define OX_ENABLE_ASSERTS
#else
	#define OX_DEBUGBREAK()
#endif

#define OX_EXPAND_MACRO(x) x
#define OX_STRINGIFY_MACRO(x) #x

#define OX_CONCAT_IMPL(a, b) a##b
#define OX_CONCAT(a, b) OX_CONCAT_IMPL(a, b)

#define BIT(x) (1 << x)

#define OX_BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

namespace oryx 
{
    // Core version
    constexpr int32_t VERSION_MAJOR = 0;
    constexpr int32_t VERSION_MINOR = 1;
    constexpr int32_t VERSION_PATCH = 0;
}

#include "Oryx/Core/Log.h"
#include "Oryx/Core/Assert.h"