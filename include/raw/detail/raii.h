#pragma once

#include <cstddef>
#include <memory>
#include <utility>

#include "memory.h"

namespace raw::detail
{

template <typename T>
class memory_guard
{
public:
	memory_guard(T* ptr, std::size_t count) noexcept
		: m_ptr(ptr)
		, m_size(count)
	{
	}

	memory_guard(const memory_guard&) = delete;

	memory_guard(memory_guard&& other) noexcept
		: m_ptr(std::exchange(other.m_ptr, nullptr))
		, m_size(std::exchange(other.m_size, 0))
	{
	}

	~memory_guard()
	{
		if (m_ptr)
		{
			deallocate(m_ptr, m_size);
		}
	}

	memory_guard& operator=(const memory_guard&) = delete;

	memory_guard& operator=(memory_guard&& other) noexcept
	{
		if (this != std::addressof(other))
		{
			if (m_ptr)
			{
				deallocate(m_ptr, m_size);
			}

			m_ptr = std::exchange(other.m_ptr, nullptr);
			m_size = std::exchange(other.m_size, 0);
		}

		return *this;
	}

	[[nodiscard]] T* get() const noexcept
	{
		return m_ptr;
	}

	[[nodiscard]] std::size_t size() const noexcept
	{
		return m_size;
	}

	void release() noexcept
	{
		m_ptr = nullptr;
		m_size = 0;
	}

private:
	T* m_ptr;
	std::size_t m_size;
};

template <typename T>
class destroy_guard
{
public:
	destroy_guard(T* first, T* last) noexcept
		: m_begin(first)
		, m_end(last)
	{
	}

	destroy_guard(const destroy_guard&) = delete;

	destroy_guard(destroy_guard&& other) noexcept
		: m_begin(std::exchange(other.m_begin, nullptr))
		, m_end(std::exchange(other.m_end, nullptr))
	{
	}

	~destroy_guard()
	{
		if (m_begin)
		{
			std::destroy(m_begin, m_end);
		}
	}

	destroy_guard& operator=(const destroy_guard&) = delete;

	destroy_guard& operator=(destroy_guard&& other) noexcept
	{
		if (this != std::addressof(other))
		{
			if (m_begin)
			{
				std::destroy(m_begin, m_end);
			}

			m_begin = std::exchange(other.m_begin, nullptr);
			m_end = std::exchange(other.m_end, nullptr);
		}

		return *this;
	}

	[[nodiscard]] T* begin() const noexcept
	{
		return m_begin;
	}

	[[nodiscard]] T* end() const noexcept
	{
		return m_end;
	}

	void setm_end(T* newm_end) noexcept
	{
		m_end = newm_end;
	}

	void release() noexcept
	{
		m_begin = nullptr;
		m_end = nullptr;
	}

private:
	T* m_begin;
	T* m_end;
};

} // namespace raw::detail
