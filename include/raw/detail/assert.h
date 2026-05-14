#pragma once

#include <cstdlib>
#include <iostream>
#include <source_location>
#include <string_view>
#include <type_traits>

#ifndef NDEBUG
#define RAW_ASSERT(expr, ...) \
        do { \
            if (std::is_constant_evaluated()) { \
                if (!(expr)) { \
                    throw "Assertion failed: " #expr; \
                } \
            } else { \
                if (!(expr)) [[unlikely]] { \
                    ::raw::detail::assert_fail(#expr, std::source_location::current() __VA_OPT__(,) __VA_ARGS__); \
                } \
            } \
        } while (0)
#else
#define RAW_ASSERT(expr, ...) ((void)sizeof(expr))
#endif

namespace raw::detail
{

[[noreturn]] inline void assert_fail(const char* expr, const std::source_location& loc, std::string_view msg = "") noexcept
{
	std::cerr << "Assertion failed: " << expr
		      << ", file " << loc.file_name()
		      << ", line " << loc.line() << '\n';
	if (!msg.empty())
	{
		std::cerr << "Message: " << msg << '\n';
	}
	std::cerr << std::flush;
	std::abort();
}

} // namespace raw::detail
