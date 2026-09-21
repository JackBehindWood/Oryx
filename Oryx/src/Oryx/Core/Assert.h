#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Core/Log.h"

namespace oryx
{

using AssertionHandler = void (*)(std::string_view message);

void set_assertion_handler(AssertionHandler handler);
[[nodiscard]] AssertionHandler assertion_handler();

// Default policy: logs the failure to the core logger, then breaks into the debugger (Debug builds).
void trap_on_assertion(std::string_view message);

// Throws AssertionError and does not log: whoever catches it reports it.
void throw_on_assertion(std::string_view message);

void assertion_failed(std::string_view message);
void assertion_failed_expression(const char* expression, const char* file, int32_t line);

[[noreturn]] void check_failed(std::string_view message);

// Always compiled and always throws AssertionError, whatever the assertion handler is.
inline void check(bool condition, std::string_view message = {})
{
    if (condition) [[likely]]
    {
        return;
    }
    check_failed(message);
}

} // namespace oryx

#ifdef OX_ENABLE_ASSERTS

	#define OX_INTERNAL_ASSERT_WITH_MSG(check, ...) { if(!(check)) { ::oryx::assertion_failed(__VA_ARGS__); } }
	#define OX_INTERNAL_ASSERT_NO_MSG(check) { if(!(check)) { ::oryx::assertion_failed_expression(OX_STRINGIFY_MACRO(check), __FILE__, __LINE__); } }

	#define OX_INTERNAL_ASSERT_GET_MACRO_NAME(arg1, arg2, macro, ...) macro
	#define OX_INTERNAL_ASSERT_GET_MACRO(...) OX_EXPAND_MACRO( OX_INTERNAL_ASSERT_GET_MACRO_NAME(__VA_ARGS__, OX_INTERNAL_ASSERT_WITH_MSG, OX_INTERNAL_ASSERT_NO_MSG) )

	// Accepts the condition and an optional message; both spellings run the same assertion handler.
	#define OX_ASSERT(...) OX_EXPAND_MACRO( OX_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(__VA_ARGS__) )
	#define OX_CORE_ASSERT(...) OX_ASSERT(__VA_ARGS__)
#else
	#define OX_ASSERT(...)
	#define OX_CORE_ASSERT(...)
#endif
