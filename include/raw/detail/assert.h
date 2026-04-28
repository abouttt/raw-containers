#pragma once

#include <cstdlib>
#include <iostream>
#include <source_location>
#include <string_view>
#include <type_traits>

#ifndef NDEBUG
#define RAW_ASSERT(expr, ...) \
		(std::is_constant_evaluated() ? \
			((expr) ? void(0) : throw "Assertion failed: " #expr) : \
			((expr) ? void(0) : raw::detail::assert_fail(#expr, std::source_location::current() __VA_OPT__(,) __VA_ARGS__)))
#else
#define RAW_ASSERT(expr, ...) ((void)0)
#endif

namespace raw::detail
{

[[noreturn]] inline void assert_fail(const char* expr, const std::source_location& loc, std::string_view msg = "") noexcept
{
	std::cerr << "Assertion failed!" << '\n';
	std::cerr << "Expression: " << expr << '\n';
	if (!msg.empty())
	{
		std::cerr << "Message: " << msg << '\n';
	}
	std::cerr << "File: " << loc.file_name() << '\n';
	std::cerr << "Line: " << loc.line() << '\n';
	std::cerr << "Function: " << loc.function_name() << '\n';
	std::cerr << std::flush;
	std::abort();
}

} // namespace raw::detail
