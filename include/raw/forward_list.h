#pragma once

#include <algorithm>
#include <compare>
#include <concepts>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

#include "detail/assert.h"
#include "detail/memory.h"
#include "detail/raii.h"

namespace raw
{

template <typename T>
class forward_list;

namespace detail
{

struct forward_list_node_base
{
	forward_list_node_base* next = nullptr;
};

template <typename T>
struct forward_list_node : forward_list_node_base
{
	T value;

	template <typename... Args>
	forward_list_node(Args&&... args)
		: forward_list_node_base{}
		, value(std::forward<Args>(args)...)
	{
	}
};

template <typename T>
class forward_list_node_guard
{
private:
	using node = forward_list_node<T>;

public:
	explicit forward_list_node_guard(node* head) noexcept
		: m_head(head)
	{
	}

	forward_list_node_guard(const forward_list_node_guard&) = delete;

	forward_list_node_guard(forward_list_node_guard&& other) noexcept
		: m_head(std::exchange(other.m_head, nullptr))
	{
	}

	~forward_list_node_guard()
	{
		destroy_node_chain();
	}

	forward_list_node_guard& operator=(const forward_list_node_guard&) = delete;

	forward_list_node_guard& operator=(forward_list_node_guard&& other) noexcept
	{
		if (this != std::addressof(other))
		{
			destroy_node_chain();
			m_head = std::exchange(other.m_head, nullptr);
		}

		return *this;
	}

	[[nodiscard]] node* get() const noexcept
	{
		return m_head;
	}

	void release() noexcept
	{
		m_head = nullptr;
	}

private:
	void destroy_node_chain()
	{
		while (m_head)
		{
			node* next = static_cast<node*>(m_head->next);
			std::destroy_at(m_head);
			detail::deallocate(m_head, 1);
			m_head = next;
		}
	}

	node* m_head;
};

template <typename T>
class forward_list_iterator
{
private:
	using node_base = forward_list_node_base;
	using node		= forward_list_node<T>;

public:
	using iterator_category = std::forward_iterator_tag;
	using value_type		= std::remove_cv_t<T>;
	using difference_type	= std::ptrdiff_t;
	using pointer			= T*;
	using reference			= T&;

	forward_list_iterator() noexcept
		: m_ptr(nullptr)
	{
	}

	explicit forward_list_iterator(node_base* ptr) noexcept
		: m_ptr(ptr)
	{
	}

	template <typename U>
		requires std::convertible_to<U*, pointer>
	forward_list_iterator(const forward_list_iterator<U>& other) noexcept
		: m_ptr(other.m_ptr)
	{
	}

	[[nodiscard]] reference operator*() const noexcept
	{
		return static_cast<node*>(m_ptr)->value;
	}

	[[nodiscard]] pointer operator->() const noexcept
	{
		return &static_cast<node*>(m_ptr)->value;
	}

	forward_list_iterator& operator++() noexcept
	{
		m_ptr = m_ptr->next;
		return *this;
	}

	forward_list_iterator operator++(int) noexcept
	{
		forward_list_iterator tmp = *this;
		++*this;
		return tmp;
	}

	template <typename U>
	[[nodiscard]] bool operator==(const forward_list_iterator<U>& other) const noexcept
	{
		return m_ptr == other.m_ptr;
	}

	template <typename U>
	[[nodiscard]] bool operator!=(const forward_list_iterator<U>& other) const noexcept
	{
		return !(*this == other);
	}

private:
	template <typename>
	friend class forward_list_iterator;
	friend class forward_list<std::remove_cv_t<T>>;

	node_base* m_ptr;
};

} // namespace detail

template <typename T>
class forward_list
{
private:
	using node_base  = detail::forward_list_node_base;
	using node		 = detail::forward_list_node<T>;
	using node_guard = detail::forward_list_node_guard<T>;

public:
	static_assert(std::is_object_v<T>, "forward_list<T> element type must be an object");

	// ---------- Types ---------- //

	using value_type	  = T;
	using size_type		  = std::size_t;
	using difference_type = std::ptrdiff_t;
	using reference		  = T&;
	using const_reference = const T&;
	using pointer		  = T*;
	using const_pointer   = const T*;

	using iterator		 = detail::forward_list_iterator<T>;
	using const_iterator = detail::forward_list_iterator<const T>;

	// ---------- Constructors / Destructor ---------- //

	forward_list() noexcept
		: m_head(nullptr)
	{
	}

	explicit forward_list(size_type count)
		: forward_list()
	{
		construct_n(count);
	}

	forward_list(size_type count, const value_type& value)
		: forward_list()
	{
		construct_n(count, value);
	}

	template <std::input_iterator InputIt>
	forward_list(InputIt first, InputIt last)
		: forward_list()
	{
		construct(first, last);
	}

	forward_list(std::initializer_list<value_type> init)
		: forward_list(init.begin(), init.end())
	{
	}

	forward_list(const forward_list& other)
		: forward_list(other.begin(), other.end())
	{
	}

	forward_list(forward_list&& other) noexcept
		: forward_list()
	{
		swap(other);
	}

	~forward_list()
	{
		tidy();
	}

	// ---------- Assignment ---------- //

	forward_list& operator=(const forward_list& other)
	{
		if (this != std::addressof(other))
		{
			assign(other.begin(), other.end());
		}

		return *this;
	}

	forward_list& operator=(forward_list&& other) noexcept
	{
		if (this != std::addressof(other))
		{
			tidy();
			swap(other);
		}

		return *this;
	}

	forward_list& operator=(std::initializer_list<value_type> ilist)
	{
		assign(ilist.begin(), ilist.end());
		return *this;
	}

	void assign(size_type count, const value_type& value)
	{
		forward_list tmp(count, value);
		swap(tmp);
	}

	template <std::input_iterator InputIt>
	void assign(InputIt first, InputIt last)
	{
		forward_list tmp(first, last);
		swap(tmp);
	}

	void assign(std::initializer_list<value_type> ilist)
	{
		assign(ilist.begin(), ilist.end());
	}

	// ---------- Element access ---------- //

	[[nodiscard]] reference front()
	{
		RAW_ASSERT(!empty(), "front() called on empty forward_list");
		return static_cast<node*>(m_head.next)->value;
	}

	[[nodiscard]] const_reference front() const
	{
		RAW_ASSERT(!empty(), "front() called on empty forward_list");
		return static_cast<const node*>(m_head.next)->value;
	}

	// ---------- Iterators ---------- //

	[[nodiscard]] iterator before_begin() noexcept
	{
		return iterator(&m_head);
	}

	[[nodiscard]] const_iterator before_begin() const noexcept
	{
		return const_iterator(const_cast<node_base*>(&m_head));
	}

	[[nodiscard]] iterator begin() noexcept
	{
		return iterator(m_head.next);
	}

	[[nodiscard]] const_iterator begin() const noexcept
	{
		return const_iterator(m_head.next);
	}

	[[nodiscard]] iterator end() noexcept
	{
		return iterator(nullptr);
	}

	[[nodiscard]] const_iterator end() const noexcept
	{
		return const_iterator(nullptr);
	}

	[[nodiscard]] const_iterator cbefore_begin() const noexcept
	{
		return before_begin();
	}

	[[nodiscard]] const_iterator cbegin() const noexcept
	{
		return begin();
	}

	[[nodiscard]] const_iterator cend() const noexcept
	{
		return end();
	}

	// ---------- Capacity ---------- //

	[[nodiscard]] bool empty() const noexcept
	{
		return m_head.next == nullptr;
	}

	[[nodiscard]] size_type max_size() const noexcept
	{
		return std::numeric_limits<difference_type>::max();
	}

	// ---------- Modifiers ---------- //

	void clear() noexcept
	{
		tidy();
	}

	iterator insert_after(const_iterator pos, const value_type& value)
	{
		return emplace_after(pos, value);
	}

	iterator insert_after(const_iterator pos, value_type&& value)
	{
		return emplace_after(pos, std::move(value));
	}

	iterator insert_after(const_iterator pos, size_type count, const value_type& value)
	{
		if (count == 0)
		{
			return iterator(pos.m_ptr);
		}

		auto [head, tail] = create_node_chain_n(count, value);
		hook_node_chain_after(pos.m_ptr, head, tail);

		return iterator(tail);
	}

	template <std::input_iterator InputIt>
	iterator insert_after(const_iterator pos, InputIt first, InputIt last)
	{
		if (first == last)
		{
			return iterator(pos.m_ptr);
		}

		auto [head, tail] = create_node_chain(first, last);
		hook_node_chain_after(pos.m_ptr, head, tail);

		return iterator(tail);
	}

	iterator insert_after(const_iterator pos, std::initializer_list<value_type> ilist)
	{
		return insert_after(pos, ilist.begin(), ilist.end());
	}

	template <typename... Args>
	iterator emplace_after(const_iterator pos, Args&&... args)
	{
		node* new_node = create_node(std::forward<Args>(args)...);
		hook_node_after(pos.m_ptr, new_node);
		return iterator(new_node);
	}

	iterator erase_after(const_iterator pos)
	{
		node_base* next = pos.m_ptr->next;
		node_base* unhooked = unhook_node_after(pos.m_ptr);
		destroy_node(static_cast<node*>(unhooked));
		return iterator(next);
	}

	iterator erase_after(const_iterator first, const_iterator last)
	{
		if (first == last)
		{
			return iterator(last.m_ptr);
		}

		node_base* next = last.m_ptr;
		auto [head, tail] = unhook_node_chain_after(first.m_ptr, last.m_ptr);
		destroy_node_chain(static_cast<node*>(head));

		return iterator(next);
	}

	void push_front(const value_type& value)
	{
		emplace_front(value);
	}

	void push_front(value_type&& value)
	{
		emplace_front(std::move(value));
	}

	template <typename... Args>
	reference emplace_front(Args&&... args)
	{
		return *emplace_after(before_begin(), std::forward<Args>(args)...);
	}

	void pop_front()
	{
		RAW_ASSERT(!empty(), "pop_front() called on empty forward_list");
		erase_after(before_begin());
	}

	void resize(size_type count)
	{
		node_base* curr = &m_head;
		size_type i = 0;

		while (curr->next && i < count)
		{
			curr = curr->next;
			++i;
		}

		if (i == count)
		{
			if (curr->next)
			{
				destroy_node_chain(static_cast<node*>(curr->next));
				curr->next = nullptr;
			}
		}
		else
		{
			auto [head, tail] = create_node_chain_n(count - i);
			hook_node_chain_after(curr, head, tail);
		}
	}

	void resize(size_type count, const value_type& value)
	{
		node_base* curr = &m_head;
		size_type i = 0;

		while (curr->next && i < count)
		{
			curr = curr->next;
			++i;
		}

		if (i == count)
		{
			if (curr->next)
			{
				destroy_node_chain(static_cast<node*>(curr->next));
				curr->next = nullptr;
			}
		}
		else
		{
			auto [head, tail] = create_node_chain_n(count - i, value);
			hook_node_chain_after(curr, head, tail);
		}
	}

	void swap(forward_list& other) noexcept
	{
		using std::swap;
		swap(m_head.next, other.m_head.next);
	}

	// ---------- Operations ---------- //

	void merge(forward_list& other)
	{
		merge(other, std::less<>{});
	}

	void merge(forward_list&& other)
	{
		merge(other);
	}

	template <typename Compare>
	void merge(forward_list& other, Compare comp)
	{
		if (this == std::addressof(other) || other.empty())
		{
			return;
		}

		iterator this_it = before_begin();
		iterator other_it = other.before_begin();

		while (this_it.m_ptr->next && other_it.m_ptr->next)
		{
			node* this_node = static_cast<node*>(this_it.m_ptr->next);
			node* other_node = static_cast<node*>(other_it.m_ptr->next);

			if (comp(other_node->value, this_node->value))
			{
				splice_after(this_it, other, other_it);
				this_it.m_ptr = this_it.m_ptr->next;
			}
			else
			{
				this_it.m_ptr = this_it.m_ptr->next;
			}
		}

		if (other_it.m_ptr->next)
		{
			splice_after(this_it, other, other_it, other.end());
		}
	}

	template <typename Compare>
	void merge(forward_list&& other, Compare comp)
	{
		merge(other, comp);
	}

	void splice_after(const_iterator pos, forward_list& other)
	{
		if (this == std::addressof(other) || other.empty())
		{
			return;
		}

		auto [head, tail] = other.unhook_node_chain_after(&other.m_head, nullptr);
		if (head)
		{
			hook_node_chain_after(pos.m_ptr, head, tail);
		}
	}

	void splice_after(const_iterator pos, forward_list&& other)
	{
		splice_after(pos, other);
	}

	void splice_after(const_iterator pos, forward_list& other, const_iterator it)
	{
		if (pos.m_ptr == it.m_ptr || pos.m_ptr == it.m_ptr->next)
		{
			return;
		}

		node_base* unhooked = other.unhook_node_after(it.m_ptr);
		if (unhooked)
		{
			hook_node_after(pos.m_ptr, unhooked);
		}
	}

	void splice_after(const_iterator pos, forward_list&& other, const_iterator it)
	{
		splice_after(pos, other, it);
	}

	void splice_after(const_iterator pos, forward_list& other, const_iterator first, const_iterator last)
	{
		if (first == last)
		{
			return;
		}

		auto [head, tail] = other.unhook_node_chain_after(first.m_ptr, last.m_ptr);
		if (head)
		{
			hook_node_chain_after(pos.m_ptr, head, tail);
		}
	}

	void splice_after(const_iterator pos, forward_list&& other, const_iterator first, const_iterator last)
	{
		splice_after(pos, other, first, last);
	}

	size_type remove(const value_type& value)
	{
		return remove_if([&](const value_type& elem) { return elem == value; });
	}

	template <typename UnaryPred>
	size_type remove_if(UnaryPred p)
	{
		iterator it = before_begin();
		size_type removed = 0;

		while (it.m_ptr->next)
		{
			node* next_node = static_cast<node*>(it.m_ptr->next);

			if (p(next_node->value))
			{
				erase_after(it);
				++removed;
			}
			else
			{
				it.m_ptr = it.m_ptr->next;
			}
		}

		return removed;
	}

	void reverse() noexcept
	{
		if (m_head.next == nullptr || m_head.next->next == nullptr)
		{
			return;
		}

		node_base* curr = m_head.next;

		while (curr->next)
		{
			node_base* unhooked = unhook_node_after(curr);
			hook_node_after(&m_head, unhooked);
		}
	}

	size_type unique()
	{
		return unique(std::equal_to<>{});
	}

	template <typename BinaryPred>
	size_type unique(BinaryPred p)
	{
		if (empty())
		{
			return 0;
		}

		iterator it = before_begin();
		size_type removed = 0;

		while (it.m_ptr->next && it.m_ptr->next->next)
		{
			node* curr_node = static_cast<node*>(it.m_ptr->next);
			node* next_node = static_cast<node*>(it.m_ptr->next->next);

			if (p(curr_node->value, next_node->value))
			{
				erase_after(iterator(it.m_ptr->next));
				++removed;
			}
			else
			{
				it.m_ptr = it.m_ptr->next;
			}
		}

		return removed;
	}

	void sort()
	{
		sort(std::less<>{});
	}

	template <typename Compare>
	void sort(Compare comp)
	{
		if (m_head.next == nullptr || m_head.next->next == nullptr)
		{
			return;
		}

		forward_list carry;
		forward_list bins[64];
		size_type fill = 0;

		while (!empty())
		{
			carry.splice_after(carry.before_begin(), *this, before_begin());

			size_type i = 0;
			while (i < fill && !bins[i].empty())
			{
				bins[i].merge(carry, comp);
				carry.swap(bins[i]);
				++i;
			}

			carry.swap(bins[i]);

			if (i == fill)
			{
				++fill;
			}
		}

		if (fill > 0)
		{
			for (size_type i = 1; i < fill; ++i)
			{
				bins[i].merge(bins[i - 1], comp);
			}

			swap(bins[fill - 1]);
		}
	}

private:
	template <typename... Args>
	[[nodiscard]] node* create_node(Args&&... args) const
	{
		node* ptr = detail::allocate<node>(1);
		detail::memory_guard<node> guard(ptr, 1);
		std::construct_at(ptr, std::forward<Args>(args)...);
		guard.release();
		return ptr;
	}

	template <std::input_iterator InputIt>
	std::pair<node*, node*> create_node_chain(InputIt first, InputIt last) const
	{
		if (first == last)
		{
			return { nullptr, nullptr };
		}

		node* head = create_node(*first);
		node* tail = head;
		node_guard guard(head);

		for (++first; first != last; ++first)
		{
			node* new_node = create_node(*first);
			tail->next = new_node;
			tail = new_node;
		}

		guard.release();

		return { head, tail };
	}

	std::pair<node*, node*> create_node_chain_n(size_type count) const
	{
		if (count == 0)
		{
			return { nullptr, nullptr };
		}

		node* head = create_node();
		node* tail = head;
		node_guard guard(head);

		for (--count; count > 0; --count)
		{
			node* new_node = create_node();
			tail->next = new_node;
			tail = new_node;
		}

		guard.release();

		return { head, tail };
	}

	std::pair<node*, node*> create_node_chain_n(size_type count, const value_type& value) const
	{
		if (count == 0)
		{
			return { nullptr, nullptr };
		}

		node* head = create_node(value);
		node* tail = head;
		node_guard guard(head);

		for (--count; count > 0; --count)
		{
			node* new_node = create_node(value);
			tail->next = new_node;
			tail = new_node;
		}

		guard.release();

		return { head, tail };
	}

	void destroy_node(node* node_to_destroy) const
	{
		std::destroy_at(node_to_destroy);
		detail::deallocate(node_to_destroy, 1);
	}

	void destroy_node_chain(node* head) const
	{
		while (head)
		{
			node* next = static_cast<node*>(head->next);
			destroy_node(head);
			head = next;
		}
	}

	void hook_node_after(node_base* pos, node_base* node_to_hook) const
	{
		node_to_hook->next = pos->next;
		pos->next = node_to_hook;
	}

	void hook_node_chain_after(node_base* pos, node_base* head, node_base* tail) const
	{
		tail->next = pos->next;
		pos->next = head;
	}

	node_base* unhook_node_after(node_base* pos) const
	{
		node_base* unhooked = pos->next;
		if (unhooked)
		{
			pos->next = unhooked->next;
			unhooked->next = nullptr;
		}

		return unhooked;
	}

	std::pair<node_base*, node_base*> unhook_node_chain_after(node_base* pos, node_base* last) const
	{
		node_base* head = pos->next;
		if (head == last)
		{
			return { nullptr, nullptr };
		}

		node_base* tail = head;
		while (tail->next != last)
		{
			tail = tail->next;
		}

		pos->next = last;
		tail->next = nullptr;

		return { head, tail };
	}

	template <std::input_iterator InputIt>
	void construct(InputIt first, InputIt last)
	{
		if (first == last)
		{
			return;
		}

		auto [head, tail] = create_node_chain(first, last);
		change_chain(head);
	}

	void construct_n(size_type count)
	{
		if (count == 0)
		{
			return;
		}

		auto [head, tail] = create_node_chain_n(count);
		change_chain(head);
	}

	void construct_n(size_type count, const value_type& value)
	{
		if (count == 0)
		{
			return;
		}

		auto [head, tail] = create_node_chain_n(count, value);
		change_chain(head);
	}

	void change_chain(node_base* new_head)
	{
		if (m_head.next)
		{
			destroy_node_chain(static_cast<node*>(m_head.next));
		}

		if (new_head)
		{
			m_head.next = new_head;
		}
		else
		{
			m_head.next = nullptr;
		}
	}

	void tidy()
	{
		change_chain(nullptr);
	}

	// ---------- Variables ---------- //

	node_base m_head;
};

// ---------- Non-member functions ---------- //

template <typename T>
[[nodiscard]] bool operator==(const forward_list<T>& lhs, const forward_list<T>& rhs)
{
	return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <typename T>
[[nodiscard]] auto operator<=>(const forward_list<T>& lhs, const forward_list<T>& rhs)
{
	return std::lexicographical_compare_three_way(
		lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), std::compare_three_way{});
}

template <typename T>
void swap(forward_list<T>& lhs, forward_list<T>& rhs) noexcept(noexcept(lhs.swap(rhs)))
{
	lhs.swap(rhs);
}

template <typename T, typename U>
typename forward_list<T>::size_type erase(forward_list<T>& c, const U& value)
{
	return c.remove_if([&](const auto& elem) -> bool { return elem == value; });
}

template <typename T, typename Pred>
typename forward_list<T>::size_type erase_if(forward_list<T>& c, Pred pred)
{
	return c.remove_if(pred);
}

// ---------- Deduction guides ---------- //

template <std::input_iterator InputIt>
forward_list(InputIt, InputIt) -> forward_list<std::iter_value_t<InputIt>>;

} // namespace raw
