#pragma once

#include <algorithm>
#include <compare>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "detail/assert.h"
#include "detail/guard.h"
#include "detail/memory.h"

namespace raw
{

namespace detail
{

template <typename T>
class vector_iterator
{
public:
	using iterator_concept	= std::contiguous_iterator_tag;
	using iterator_category = std::random_access_iterator_tag;
	using value_type		= std::remove_cv_t<T>;
	using difference_type	= std::ptrdiff_t;
	using pointer			= T*;
	using reference			= T&;

	vector_iterator() noexcept
		: m_ptr(nullptr)
	{
	}

	explicit vector_iterator(pointer ptr) noexcept
		: m_ptr(ptr)
	{
	}

	template <typename U>
		requires std::convertible_to<U*, pointer>
	vector_iterator(const vector_iterator<U>& other) noexcept
		: m_ptr(other.m_ptr)
	{
	}

	[[nodiscard]] reference operator*() const noexcept
	{
		return *m_ptr;
	}

	[[nodiscard]] pointer operator->() const noexcept
	{
		return m_ptr;
	}

	vector_iterator& operator++() noexcept
	{
		++m_ptr;
		return *this;
	}

	vector_iterator operator++(int) noexcept
	{
		vector_iterator tmp = *this;
		++m_ptr;
		return tmp;
	}

	vector_iterator& operator--() noexcept
	{
		--m_ptr;
		return *this;
	}

	vector_iterator operator--(int) noexcept
	{
		vector_iterator tmp = *this;
		--m_ptr;
		return tmp;
	}

	vector_iterator& operator+=(difference_type n) noexcept
	{
		m_ptr += n;
		return *this;
	}

	[[nodiscard]] vector_iterator operator+(difference_type n) const noexcept
	{
		return vector_iterator(m_ptr + n);
	}

	[[nodiscard]] friend vector_iterator operator+(difference_type n, const vector_iterator& it) noexcept
	{
		return vector_iterator(it.m_ptr + n);
	}

	vector_iterator& operator-=(difference_type n) noexcept
	{
		m_ptr -= n;
		return *this;
	}

	[[nodiscard]] vector_iterator operator-(difference_type n) const noexcept
	{
		return vector_iterator(m_ptr - n);
	}

	template <typename U>
	[[nodiscard]] difference_type operator-(const vector_iterator<U>& other) const noexcept
	{
		return m_ptr - other.m_ptr;
	}

	[[nodiscard]] reference operator[](difference_type n) const noexcept
	{
		return *(m_ptr + n);
	}

	template <typename U>
	[[nodiscard]] bool operator==(const vector_iterator<U>& other) const noexcept
	{
		return m_ptr == other.m_ptr;
	}

	template <typename U>
	[[nodiscard]] std::strong_ordering operator<=>(const vector_iterator<U>& other) const noexcept
	{
		return m_ptr <=> other.m_ptr;
	}

private:
	template <typename>
	friend class vector_iterator;

	pointer m_ptr;
};

} // namespace detail

template <typename T>
class vector
{
private:
	using memory_guard = detail::memory_guard<T>;
	using object_guard = detail::object_guard<T>;

	static constexpr auto alloc = detail::allocate<T>;
	static constexpr auto free	= detail::deallocate<T>;

public:
	static_assert(std::is_object_v<T>, "vector<T> element type must be an object");

	// ---------- Types ---------- //

	using value_type	  = T;
	using size_type		  = std::size_t;
	using difference_type = std::ptrdiff_t;
	using reference		  = T&;
	using const_reference = const T&;
	using pointer		  = T*;
	using const_pointer   = const T*;

	using iterator				 = detail::vector_iterator<T>;
	using const_iterator		 = detail::vector_iterator<const T>;
	using reverse_iterator		 = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	// ---------- Constructors / Destructor ---------- //

	vector() noexcept
		: m_begin(nullptr)
		, m_end(nullptr)
		, m_end_cap(nullptr)
	{
	}

	explicit vector(size_type count)
		: vector()
	{
		construct_n(count);
	}

	vector(size_type count, const value_type& value)
		: vector()
	{
		construct_n(count, value);
	}

	template <std::input_iterator InputIt>
	vector(InputIt first, InputIt last)
		: vector()
	{
		if constexpr (std::forward_iterator<InputIt>)
		{
			const size_type count = range_to_count(first, last);
			construct_n(count, first, last);
		}
		else
		{
			for (; first != last; ++first)
			{
				emplace_back(*first);
			}
		}
	}

	vector(std::initializer_list<value_type> init)
		: vector(init.begin(), init.end())
	{
	}

	vector(const vector& other)
		: vector(other.begin(), other.end())
	{
	}

	vector(vector&& other) noexcept
		: m_begin(std::exchange(other.m_begin, nullptr))
		, m_end(std::exchange(other.m_end, nullptr))
		, m_end_cap(std::exchange(other.m_end_cap, nullptr))
	{
	}

	~vector()
	{
		tidy();
	}

	// ---------- Assignment ---------- //

	vector& operator=(const vector& other)
	{
		if (this != std::addressof(other))
		{
			assign(other.begin(), other.end());
		}

		return *this;
	}

	vector& operator=(vector&& other) noexcept
	{
		if (this != std::addressof(other))
		{
			tidy();
			swap(other);
		}

		return *this;
	}

	vector& operator=(std::initializer_list<value_type> ilist)
	{
		assign(ilist.begin(), ilist.end());
		return *this;
	}

	void assign(size_type count, const value_type& value)
	{
		if (count <= capacity())
		{
			const size_type old_size = size();

			if (count > old_size)
			{
				std::fill_n(m_begin, old_size, value);
				std::uninitialized_fill_n(m_end, count - old_size, value);
			}
			else
			{
				std::fill_n(m_begin, count, value);
				std::destroy_n(m_begin + count, old_size - count);
			}

			m_end = m_begin + count;
		}
		else
		{
			if (count > max_size())
			{
				xlength();
			}

			const size_type new_cap = calculate_growth(count);

			pointer new_begin = alloc(new_cap);
			memory_guard mem_guard(new_begin, new_cap);

			std::uninitialized_fill_n(new_begin, count, value);

			mem_guard.release();

			change_array(new_begin, count, new_cap);
		}
	}

	template <std::input_iterator InputIt>
	void assign(InputIt first, InputIt last)
	{
		if constexpr (std::forward_iterator<InputIt>)
		{
			const size_type count = range_to_count(first, last);

			if (count <= capacity())
			{
				const size_type old_size = size();

				if (count > old_size)
				{
					InputIt mid = std::next(first, old_size);
					std::copy(first, mid, m_begin);
					std::uninitialized_copy(mid, last, m_end);
				}
				else
				{
					std::copy(first, last, m_begin);
					std::destroy(m_begin + count, m_end);
				}

				m_end = m_begin + count;
			}
			else
			{
				if (count > max_size())
				{
					xlength();
				}

				const size_type new_cap = calculate_growth(count);

				pointer new_begin = alloc(new_cap);
				memory_guard mem_guard(new_begin, new_cap);

				std::uninitialized_copy(first, last, new_begin);

				mem_guard.release();

				change_array(new_begin, count, new_cap);
			}
		}
		else
		{
			vector(first, last).swap(*this);
		}
	}

	void assign(std::initializer_list<value_type> ilist)
	{
		assign(ilist.begin(), ilist.end());
	}

	// ---------- Element access ---------- //

	[[nodiscard]] reference at(size_type pos)
	{
		if (pos >= size())
		{
			xrange();
		}

		return m_begin[pos];
	}

	[[nodiscard]] const_reference at(size_type pos) const
	{
		if (pos >= size())
		{
			xrange();
		}

		return m_begin[pos];
	}

	[[nodiscard]] reference operator[](size_type pos)
	{
		RAW_ASSERT(pos < size(), "vector subscript out of range");
		return m_begin[pos];
	}

	[[nodiscard]] const_reference operator[](size_type pos) const
	{
		RAW_ASSERT(pos < size(), "vector subscript out of range");
		return m_begin[pos];
	}

	[[nodiscard]] reference front()
	{
		RAW_ASSERT(!empty(), "front() called on empty vector");
		return *m_begin;
	}

	[[nodiscard]] const_reference front() const
	{
		RAW_ASSERT(!empty(), "front() called on empty vector");
		return *m_begin;
	}

	[[nodiscard]] reference back()
	{
		RAW_ASSERT(!empty(), "back() called on empty vector");
		return *(m_end - 1);
	}

	[[nodiscard]] const_reference back() const
	{
		RAW_ASSERT(!empty(), "back() called on empty vector");
		return *(m_end - 1);
	}

	[[nodiscard]] pointer data() noexcept
	{
		return m_begin;
	}

	[[nodiscard]] const_pointer data() const noexcept
	{
		return m_begin;
	}

	// ---------- Iterators ---------- //

	[[nodiscard]] iterator begin() noexcept
	{
		return iterator(m_begin);
	}

	[[nodiscard]] const_iterator begin() const noexcept
	{
		return const_iterator(m_begin);
	}

	[[nodiscard]] iterator end() noexcept
	{
		return iterator(m_end);
	}

	[[nodiscard]] const_iterator end() const noexcept
	{
		return const_iterator(m_end);
	}

	[[nodiscard]] reverse_iterator rbegin() noexcept
	{
		return reverse_iterator(end());
	}

	[[nodiscard]] const_reverse_iterator rbegin() const noexcept
	{
		return const_reverse_iterator(end());
	}

	[[nodiscard]] reverse_iterator rend() noexcept
	{
		return reverse_iterator(begin());
	}

	[[nodiscard]] const_reverse_iterator rend() const noexcept
	{
		return const_reverse_iterator(begin());
	}

	[[nodiscard]] const_iterator cbegin() const noexcept
	{
		return begin();
	}

	[[nodiscard]] const_iterator cend() const noexcept
	{
		return end();
	}

	[[nodiscard]] const_reverse_iterator crbegin() const noexcept
	{
		return rbegin();
	}

	[[nodiscard]] const_reverse_iterator crend() const noexcept
	{
		return rend();
	}

	// ---------- Capacity ---------- //

	[[nodiscard]] bool empty() const noexcept
	{
		return m_begin == m_end;
	}

	[[nodiscard]] size_type size() const noexcept
	{
		return static_cast<size_type>(m_end - m_begin);
	}

	[[nodiscard]] size_type max_size() const noexcept
	{
		return std::numeric_limits<difference_type>::max() / sizeof(value_type);
	}

	[[nodiscard]] size_type capacity() const noexcept
	{
		return static_cast<size_type>(m_end_cap - m_begin);
	}

	void reserve(size_type new_cap)
	{
		if (new_cap > capacity())
		{
			if (new_cap > max_size())
			{
				xlength();
			}

			reallocate(new_cap);
		}
	}

	void shrink_to_fit()
	{
		if (m_end != m_end_cap)
		{
			if (m_begin == m_end)
			{
				tidy();
			}
			else
			{
				reallocate(size());
			}
		}
	}

	// ---------- Modifiers ---------- //

	void clear() noexcept
	{
		std::destroy(m_begin, m_end);
		m_end = m_begin;
	}

	iterator insert(const_iterator pos, const value_type& value)
	{
		return emplace(pos, value);
	}

	iterator insert(const_iterator pos, value_type&& value)
	{
		return emplace(pos, std::move(value));
	}

	iterator insert(const_iterator pos, size_type count, const value_type& value)
	{
		RAW_ASSERT(pos >= begin() && pos <= end(), "vector insert iterator outside range");

		const difference_type offset = pos - begin();

		if (count == 0)
		{
			return begin() + offset;
		}

		const size_type old_size = size();

		if (count + old_size <= capacity())
		{
			const size_type elems_after = old_size - offset;
			pointer insert_pos = m_begin + offset;

			if (count > elems_after)
			{
				std::uninitialized_fill_n(m_end, count - elems_after, value);
				object_guard obj_guard1(m_end, count - elems_after);

				relocate_n(insert_pos, elems_after, insert_pos + count);
				object_guard obj_guard2(insert_pos + count, elems_after);

				std::fill_n(insert_pos, elems_after, value);

				obj_guard1.release();
				obj_guard2.release();
			}
			else
			{
				relocate_n(m_end - count, count, m_end);
				object_guard obj_guard(m_end, count);

				const size_type mid_count = m_end - count - insert_pos;
				relocate_backward_n(insert_pos, mid_count, m_end);

				std::fill_n(insert_pos, count, value);

				obj_guard.release();
			}

			m_end += count;
		}
		else
		{
			if (count > max_size() - old_size)
			{
				xlength();
			}

			const size_type new_size = count + old_size;
			const size_type new_cap = calculate_growth(new_size);

			pointer new_begin = alloc(new_cap);
			memory_guard mem_guard(new_begin, new_cap);

			pointer insert_pos = new_begin + offset;

			std::uninitialized_fill_n(insert_pos, count, value);
			object_guard obj_guard1(insert_pos, count);

			relocate_n(m_begin, offset, new_begin);
			object_guard obj_guard2(new_begin, offset);

			relocate_n(m_begin + offset, old_size - offset, insert_pos + count);

			obj_guard2.release();
			obj_guard1.release();
			mem_guard.release();

			change_array(new_begin, new_size, new_cap);
		}

		return begin() + offset;
	}

	template <std::input_iterator InputIt>
	iterator insert(const_iterator pos, InputIt first, InputIt last)
	{
		RAW_ASSERT(pos >= begin() && pos <= end(), "vector insert iterator outside range");

		if constexpr (std::forward_iterator<InputIt>)
		{
			const size_type count = range_to_count(first, last);
			const difference_type offset = pos - begin();

			if (count == 0)
			{
				return begin() + offset;
			}

			const size_type old_size = size();

			if (count + old_size <= capacity())
			{
				const size_type elems_after = old_size - offset;
				pointer insert_pos = m_begin + offset;

				if (count > elems_after)
				{
					InputIt mid = std::next(first, elems_after);

					std::uninitialized_copy(mid, last, m_end);
					object_guard obj_guard1(m_end, count - elems_after);

					relocate_n(insert_pos, elems_after, insert_pos + count);
					object_guard obj_guard2(insert_pos + count, elems_after);

					std::copy(first, mid, insert_pos);

					obj_guard2.release();
					obj_guard1.release();
				}
				else
				{
					relocate_n(m_end - count, count, m_end);
					object_guard obj_guard(m_end, count);

					const size_type mid_count = (m_end - count) - insert_pos;
					relocate_backward_n(insert_pos, mid_count, m_end);

					std::copy(first, last, insert_pos);

					obj_guard.release();
				}

				m_end += count;
			}
			else
			{
				if (count > max_size() - old_size)
				{
					xlength();
				}

				const size_type new_size = count + old_size;
				const size_type new_cap = calculate_growth(new_size);

				pointer new_begin = alloc(new_cap);
				memory_guard mem_guard(new_begin, new_cap);

				pointer insert_pos = new_begin + offset;

				std::uninitialized_copy(first, last, insert_pos);
				object_guard obj_guard1(insert_pos, count);

				relocate_n(m_begin, offset, new_begin);
				object_guard obj_guard2(new_begin, offset);

				relocate_n(m_begin + offset, old_size - offset, insert_pos + count);

				obj_guard2.release();
				obj_guard1.release();
				mem_guard.release();

				change_array(new_begin, new_size, new_cap);
			}

			return begin() + offset;
		}
		else
		{
			vector tmp(first, last);
			return insert(pos, tmp.begin(), tmp.end());
		}
	}

	iterator insert(const_iterator pos, std::initializer_list<value_type> ilist)
	{
		return insert(pos, ilist.begin(), ilist.end());
	}

	template <typename... Args>
	iterator emplace(const_iterator pos, Args&&... args)
	{
		RAW_ASSERT(pos >= begin() && pos <= end(), "vector emplace iterator outside range");

		const difference_type offset = pos - begin();

		if (m_end != m_end_cap)
		{
			pointer insert_pos = m_begin + offset;

			if (insert_pos == m_end)
			{
				std::construct_at(m_end, std::forward<Args>(args)...);
				++m_end;
			}
			else
			{
				value_type tmp(std::forward<Args>(args)...);

				std::construct_at(m_end, std::move(*(m_end - 1)));
				++m_end;

				std::move_backward(insert_pos, m_end - 2, m_end - 1);
				*insert_pos = std::move(tmp);
			}
		}
		else
		{
			const size_type old_size = size();

			if (old_size == max_size())
			{
				xlength();
			}

			const size_type new_size = old_size + 1;
			const size_type new_cap = calculate_growth(new_size);

			pointer new_begin = alloc(new_cap);
			memory_guard mem_guard(new_begin, new_cap);

			pointer insert_pos = new_begin + offset;

			std::construct_at(insert_pos, std::forward<Args>(args)...);
			object_guard obj_guard1(insert_pos, 1);

			relocate_n(m_begin, offset, new_begin);
			object_guard obj_guard2(new_begin, offset);

			relocate_n(m_begin + offset, old_size - offset, insert_pos + 1);

			obj_guard2.release();
			obj_guard1.release();
			mem_guard.release();

			change_array(new_begin, new_size, new_cap);
		}

		return begin() + offset;
	}

	iterator erase(const_iterator pos)
	{
		RAW_ASSERT(pos >= begin() && pos < end(), "vector erase iterator outside range");

		const difference_type offset = pos - begin();
		pointer erase_pos = m_begin + offset;

		if (erase_pos != m_end - 1)
		{
			std::move(erase_pos + 1, m_end, erase_pos);
		}

		std::destroy_at(m_end - 1);
		--m_end;

		return iterator(erase_pos);
	}

	iterator erase(const_iterator first, const_iterator last)
	{
		RAW_ASSERT(first >= begin() && first <= last && last <= end(), "vector erase iterator outside range");

		const difference_type offset_first = first - begin();
		const difference_type offset_last = last - begin();
		pointer erase_first = m_begin + offset_first;
		pointer erase_last = m_begin + offset_last;

		if (erase_first != erase_last)
		{
			pointer new_end = std::move(erase_last, m_end, erase_first);
			std::destroy(new_end, m_end);
			m_end = new_end;
		}

		return iterator(erase_first);
	}

	void push_back(const value_type& value)
	{
		emplace_back(value);
	}

	void push_back(value_type&& value)
	{
		emplace_back(std::move(value));
	}

	template <typename... Args>
	reference emplace_back(Args&&... args)
	{
		if (m_end != m_end_cap)
		{
			std::construct_at(m_end, std::forward<Args>(args)...);
			++m_end;
		}
		else
		{
			const size_type old_size = size();

			if (old_size == max_size())
			{
				xlength();
			}

			const size_type new_size = old_size + 1;
			const size_type new_cap = calculate_growth(new_size);

			pointer new_begin = alloc(new_cap);
			memory_guard mem_guard(new_begin, new_cap);

			pointer insert_pos = new_begin + old_size;

			std::construct_at(insert_pos, std::forward<Args>(args)...);
			object_guard obj_guard(insert_pos, 1);

			relocate_n(m_begin, old_size, new_begin);

			obj_guard.release();
			mem_guard.release();

			change_array(new_begin, new_size, new_cap);
		}

		return back();
	}

	void pop_back()
	{
		RAW_ASSERT(!empty(), "pop_back() called on empty vector");
		std::destroy_at(m_end - 1);
		--m_end;
	}

	void resize(size_type count)
	{
		const size_type old_size = size();

		if (count < old_size)
		{
			std::destroy_n(m_begin + count, old_size - count);
			m_end = m_begin + count;
		}
		else if (count > old_size)
		{
			if (count <= capacity())
			{
				std::uninitialized_value_construct_n(m_end, count - old_size);
				m_end = m_begin + count;
			}
			else
			{
				if (count > max_size())
				{
					xlength();
				}

				const size_type new_cap = calculate_growth(count);

				pointer new_begin = alloc(new_cap);
				memory_guard mem_guard(new_begin, new_cap);

				relocate_n(m_begin, old_size, new_begin);

				std::uninitialized_value_construct_n(new_begin + old_size, count - old_size);
				object_guard obj_guard(new_begin + old_size, count - old_size);

				obj_guard.release();
				mem_guard.release();

				change_array(new_begin, count, new_cap);
			}
		}
	}

	void resize(size_type count, const value_type& value)
	{
		const size_type old_size = size();

		if (count < old_size)
		{
			std::destroy_n(m_begin + count, old_size - count);
			m_end = m_begin + count;
		}
		else if (count > old_size)
		{
			if (count <= capacity())
			{
				std::uninitialized_fill_n(m_end, count - old_size, value);
				m_end = m_begin + count;
			}
			else
			{
				if (count > max_size())
				{
					xlength();
				}

				const size_type new_cap = calculate_growth(count);

				pointer new_begin = alloc(new_cap);
				memory_guard mem_guard(new_begin, new_cap);

				relocate_n(m_begin, old_size, new_begin);

				std::uninitialized_fill_n(new_begin + old_size, count - old_size, value);
				object_guard obj_guard(new_begin + old_size, count - old_size);

				obj_guard.release();
				mem_guard.release();

				change_array(new_begin, count, new_cap);
			}
		}
	}

	void swap(vector& other) noexcept
	{
		if (this != std::addressof(other))
		{
			using std::swap;
			swap(m_begin, other.m_begin);
			swap(m_end, other.m_end);
			swap(m_end_cap, other.m_end_cap);
		}
	}

private:
	[[noreturn]] static void xlength()
	{
		throw std::length_error("vector too long");
	}

	[[noreturn]] static void xrange()
	{
		throw std::out_of_range("invalid vector subscript");
	}

	template <std::forward_iterator ForwardIt>
	[[nodiscard]] size_type range_to_count(ForwardIt first, ForwardIt last) const
	{
		const difference_type length = std::distance(first, last);
		return static_cast<size_type>(length);
	}

	[[nodiscard]] size_type calculate_growth(size_type new_size) const
	{
		const size_type old_cap = capacity();
		const size_type max_cap = max_size();

		if (old_cap > max_cap - old_cap / 2)
		{
			return max_cap;
		}

		const size_type geometric = old_cap + old_cap / 2;

		return geometric < new_size ? new_size : geometric;
	}

	template <typename... Args>
	void construct_n(size_type count, Args&&... args)
	{
		if (count == 0)
		{
			return;
		}

		if (count > max_size())
		{
			xlength();
		}

		pointer new_begin = alloc(count);
		memory_guard mem_guard(new_begin, count);

		if constexpr (sizeof...(args) == 0)
		{
			std::uninitialized_value_construct_n(new_begin, count);
		}
		else if constexpr (sizeof...(args) == 1)
		{
			std::uninitialized_fill_n(new_begin, count, std::forward<Args>(args)...);
		}
		else if constexpr (sizeof...(args) == 2)
		{
			std::uninitialized_copy(std::forward<Args>(args)..., new_begin);
		}
		else
		{
			static_assert(sizeof...(args) <= 2, "construct_n() supports at most 2 arguments");
		}

		mem_guard.release();

		change_array(new_begin, count, count);
	}

	void relocate_n(pointer first, size_type count, pointer dest) const
	{
		if constexpr (std::is_nothrow_move_constructible_v<value_type> || !std::is_copy_constructible_v<value_type>)
		{
			std::uninitialized_move_n(first, count, dest);
		}
		else
		{
			std::uninitialized_copy_n(first, count, dest);
		}
	}

	void relocate_backward_n(pointer first, size_type count, pointer dest)
	{
		if constexpr (std::is_nothrow_move_assignable_v<value_type>)
		{
			std::move_backward(first, first + count, dest);
		}
		else
		{
			std::copy_backward(first, first + count, dest);
			std::destroy_n(first, count);
		}
	}

	void reallocate(size_type new_cap)
	{
		const size_type new_size = std::min(size(), new_cap);
		pointer new_begin = alloc(new_cap);
		memory_guard mem_guard(new_begin, new_cap);

		relocate_n(m_begin, new_size, new_begin);

		mem_guard.release();

		change_array(new_begin, new_size, new_cap);
	}

	void change_array(pointer new_begin, size_type new_size, size_type new_cap)
	{
		if (m_begin)
		{
			std::destroy(m_begin, m_end);
			free(m_begin, capacity());
		}

		m_begin = new_begin;
		m_end = new_begin + new_size;
		m_end_cap = new_begin + new_cap;
	}

	void tidy()
	{
		change_array(nullptr, 0, 0);
	}

	// ---------- Variables ---------- //

	pointer m_begin;
	pointer m_end;
	pointer m_end_cap;
};

// ---------- Non-member functions ---------- //

template <typename T>
[[nodiscard]] bool operator==(const vector<T>& lhs, const vector<T>& rhs)
{
	if (lhs.size() != rhs.size())
	{
		return false;
	}

	return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <typename T>
[[nodiscard]] auto operator<=>(const vector<T>& lhs, const vector<T>& rhs)
{
	return std::lexicographical_compare_three_way(
		lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), std::compare_three_way{});
}

template <typename T>
void swap(vector<T>& lhs, vector<T>& rhs) noexcept(noexcept(lhs.swap(rhs)))
{
	lhs.swap(rhs);
}

template <typename T, typename U>
typename vector<T>::size_type erase(vector<T>& c, const U& value)
{
	auto it = std::remove(c.begin(), c.end(), value);
	auto r = c.end() - it;
	c.erase(it, c.end());
	return r;
}

template <typename T, typename Pred>
typename vector<T>::size_type erase_if(vector<T>& c, Pred pred)
{
	auto it = std::remove_if(c.begin(), c.end(), pred);
	auto r = c.end() - it;
	c.erase(it, c.end());
	return r;
}

// ---------- Deduction guides ---------- //

template <std::input_iterator InputIt>
vector(InputIt, InputIt) -> vector<std::iter_value_t<InputIt>>;

} // namespace raw
