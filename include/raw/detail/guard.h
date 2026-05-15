#pragma once

#include <cstddef>
#include <memory>

#include "memory.h"

namespace raw::detail
{

template <typename T>
class memory_guard
{
public:
	memory_guard(T* ptr, std::size_t count) noexcept
		: m_ptr(ptr)
		, m_count(count)
	{
	}

	~memory_guard() noexcept
	{
		if (m_ptr)
		{
			deallocate(m_ptr, m_count);
		}
	}

	memory_guard(const memory_guard&) = delete;
	memory_guard(memory_guard&&) = delete;

	memory_guard& operator=(const memory_guard&) = delete;
	memory_guard& operator=(memory_guard&&) = delete;

	[[nodiscard]] T* get() const noexcept
	{
		return m_ptr;
	}

	[[nodiscard]] std::size_t count() const noexcept
	{
		return m_count;
	}

	void release() noexcept
	{
		m_ptr = nullptr;
		m_count = 0;
	}

private:
	T* m_ptr;
	std::size_t m_count;
};

template <typename T>
class object_guard
{
public:
	object_guard(T* ptr, std::size_t count) noexcept
		: m_ptr(ptr)
		, m_count(count)
	{
	}

	~object_guard() noexcept
	{
		if (m_ptr)
		{
			std::destroy_n(m_ptr, m_count);
		}
	}

	object_guard(const object_guard&) = delete;
	object_guard(object_guard&&) = delete;

	object_guard& operator=(const object_guard&) = delete;
	object_guard& operator=(object_guard&&) = delete;

	[[nodiscard]] T* get() const noexcept
	{
		return m_ptr;
	}

	[[nodiscard]] std::size_t count() const noexcept
	{
		return m_count;
	}

	void next(std::size_t n = 1) noexcept
	{
		m_count += n;
	}

	void prev(std::size_t n = 1) noexcept
	{
		m_count -= n;
	}

	void release() noexcept
	{
		m_ptr = nullptr;
		m_count = 0;
	}

private:
	T* m_ptr;
	std::size_t m_count;
};

} // namespace raw::detail
