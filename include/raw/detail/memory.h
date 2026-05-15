#pragma once

#include <cstddef>
#include <new>

namespace raw::detail
{

template <typename T>
[[nodiscard]] T* allocate(std::size_t count)
{
	static_assert(sizeof(T) > 0, "T must be a complete type");

	if (count == 0)
	{
		return nullptr;
	}

	constexpr std::size_t max = static_cast<std::size_t>(-1) / sizeof(T);

	if (count > max)
	{
		throw std::bad_array_new_length();
	}

	constexpr std::size_t alignment = alignof(T);
	const std::size_t size = count * sizeof(T);

	if constexpr (alignment > alignof(std::max_align_t))
	{
		return static_cast<T*>(::operator new(size, std::align_val_t{ alignment }));
	}
	else
	{
		return static_cast<T*>(::operator new(size));
	}
}

template <typename T>
void deallocate(T* ptr, std::size_t count) noexcept
{
	if (!ptr)
	{
		return;
	}

	constexpr std::size_t alignment = alignof(T);
	const std::size_t size = count * sizeof(T);

	if constexpr (alignment > alignof(std::max_align_t))
	{
		::operator delete(ptr, size, std::align_val_t{ alignment });
	}
	else
	{
		::operator delete(ptr, size);
	}
}

} // namespace raw::detail
