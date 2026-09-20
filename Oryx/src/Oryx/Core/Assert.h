#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Core/Log.h"

#ifdef OX_ENABLE_ASSERTS

	// Alteratively we could use the same "default" message for both "WITH_MSG" and "NO_MSG" and
	// provide support for custom formatting by concatenating the formatting string instead of having the format inside the default message
	#define OX_INTERNAL_ASSERT_IMPL(type, check, msg, ...) { if(!(check)) { OX##type##ERROR(msg, __VA_ARGS__); OX_DEBUGBREAK(); } }
	#define OX_INTERNAL_ASSERT_WITH_MSG(type, check, ...) OX_INTERNAL_ASSERT_IMPL(type, check, "Assertion failed: {0}", __VA_ARGS__)
	#define OX_INTERNAL_ASSERT_NO_MSG(type, check) OX_INTERNAL_ASSERT_IMPL(type, check, "Assertion '{0}' failed at {1}:{2}", OX_STRINGIFY_MACRO(check), std::filesystem::path(__FILE__).filename().string(), __LINE__)

	#define OX_INTERNAL_ASSERT_GET_MACRO_NAME(arg1, arg2, macro, ...) macro
	#define OX_INTERNAL_ASSERT_GET_MACRO(...) OX_EXPAND_MACRO( OX_INTERNAL_ASSERT_GET_MACRO_NAME(__VA_ARGS__, OX_INTERNAL_ASSERT_WITH_MSG, OX_INTERNAL_ASSERT_NO_MSG) )

	// Currently accepts at least the condition and one additional parameter (the message) being optional
	#define OX_ASSERT(...) OX_EXPAND_MACRO( OX_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_, __VA_ARGS__) )
	#define OX_CORE_ASSERT(...) OX_EXPAND_MACRO( OX_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_CORE_, __VA_ARGS__) )
#else
	#define OX_ASSERT(...)
	#define OX_CORE_ASSERT(...)
#endif