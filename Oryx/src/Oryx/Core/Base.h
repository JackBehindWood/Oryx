#pragma once

#include "Oryx/Core/PlatformDetection.h"

#include <memory>

#ifdef ORYX_DEBUG
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

#define BIT(x) (1 << x)

#define OX_BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

namespace oryx 
{

	template<typename T>
	using UniquePtr = std::unique_ptr<T>;
	template<typename T, typename ... Args>
	constexpr UniquePtr<T> create_unique(Args&& ... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

	template<typename T>
	using SharedPtr = std::shared_ptr<T>;
	template<typename T, typename ... Args>
	constexpr SharedPtr<T> create_shared(Args&& ... args)
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}

    // Core version
    constexpr int32_t VERSION_MAJOR = 0;
    constexpr int32_t VERSION_MINOR = 1;
    constexpr int32_t VERSION_PATCH = 0;
}

#include "Oryx/Core/Log.h"
#include "Oryx/Core/Assert.h"