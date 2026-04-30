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
#include <tuple>
#include <type_traits>
#include <utility>

#include "detail/assert.h"
#include "detail/memory.h"
#include "detail/raii.h"

namespace raw
{

template <typename T>
class list;

namespace detail
{

struct list_node_base
{
	list_node_base* next = nullptr;
	list_node_base* prev = nullptr;
};

template <typename T>
struct list_node : list_node_base
{
	T value;

	template <typename... Args>
	list_node(Args&&... args)
		: list_node_base{}
		, value(std::forward<Args>(args)...)
	{
	}
};

template <typename T>
class list_node_guard
{
private:
	using node = list_node<T>;

public:
	explicit list_node_guard(node* head) noexcept
		: m_head(head)
	{
	}

	list_node_guard(const list_node_guard&) = delete;

	list_node_guard(list_node_guard&& other) noexcept
		: m_head(std::exchange(other.m_head, nullptr))
	{
	}

	~list_node_guard()
	{
		destroy_node_chain();
	}

	list_node_guard& operator=(const list_node_guard&) = delete;

	list_node_guard& operator=(list_node_guard&& other) noexcept
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
class list_iterator
{
private:
	using node_base = list_node_base;
	using node		= list_node<T>;

public:
	using iterator_category = std::bidirectional_iterator_tag;
	using value_type		= std::remove_cv_t<T>;
	using difference_type	= std::ptrdiff_t;
	using pointer			= T*;
	using reference			= T&;

	list_iterator() noexcept
		: m_ptr(nullptr)
	{
	}

	explicit list_iterator(node_base* ptr) noexcept
		: m_ptr(ptr)
	{
	}

	template <typename U>
		requires std::convertible_to<U*, pointer>
	list_iterator(const list_iterator<U>& other) noexcept
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

	list_iterator& operator++() noexcept
	{
		m_ptr = m_ptr->next;
		return *this;
	}

	list_iterator operator++(int) noexcept
	{
		list_iterator tmp = *this;
		++*this;
		return tmp;
	}

	list_iterator& operator--() noexcept
	{
		m_ptr = m_ptr->prev;
		return *this;
	}

	list_iterator operator--(int) noexcept
	{
		list_iterator tmp = *this;
		--*this;
		return tmp;
	}

	template <typename U>
	[[nodiscard]] bool operator==(const list_iterator<U>& other) const noexcept
	{
		return m_ptr == other.m_ptr;
	}

	template <typename U>
	[[nodiscard]] bool operator!=(const list_iterator<U>& other) const noexcept
	{
		return !(*this == other);
	}

private:
	template <typename>
	friend class list_iterator;
	friend class list<std::remove_cv_t<T>>;

	node_base* m_ptr;
};

} // namespace detail

template <typename T>
class list
{
private:
	using node_base  = detail::list_node_base;
	using node		 = detail::list_node<T>;
	using node_guard = detail::list_node_guard<T>;

public:
	static_assert(std::is_object_v<T>, "list<T> element type must be an object");

	// ---------- Types ---------- //

	using value_type	  = T;
	using size_type		  = std::size_t;
	using difference_type = std::ptrdiff_t;
	using reference		  = T&;
	using const_reference = const T&;
	using pointer		  = T*;
	using const_pointer   = const T*;

	using iterator				 = detail::list_iterator<T>;
	using const_iterator		 = detail::list_iterator<const T>;
	using reverse_iterator		 = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	// ---------- Constructors / Destructor ---------- //

	list() noexcept
		: m_sentinel{ &m_sentinel, &m_sentinel }
		, m_size(0)
	{
	}

	explicit list(size_type count)
		: list()
	{
		construct_n(count);
	}

	list(size_type count, const value_type& value)
		: list()
	{
		construct_n(count, value);
	}

	template <std::input_iterator InputIt>
	list(InputIt first, InputIt last)
		: list()
	{
		construct(first, last);
	}

	list(std::initializer_list<value_type> init)
		: list(init.begin(), init.end())
	{
	}

	list(const list& other)
		: list(other.begin(), other.end())
	{
	}

	list(list&& other) noexcept
		: list()
	{
		swap(other);
	}

	~list()
	{
		tidy();
	}

	// ---------- Assignment ---------- //

	list& operator=(const list& other)
	{
		if (this != std::addressof(other))
		{
			assign(other.begin(), other.end());
		}

		return *this;
	}

	list& operator=(list&& other) noexcept
	{
		if (this != std::addressof(other))
		{
			tidy();
			swap(other);
		}

		return *this;
	}

	list& operator=(std::initializer_list<value_type> ilist)
	{
		assign(ilist.begin(), ilist.end());
		return *this;
	}

	void assign(size_type count, const value_type& value)
	{
		list tmp(count, value);
		swap(tmp);
	}

	template <std::input_iterator InputIt>
	void assign(InputIt first, InputIt last)
	{
		list tmp(first, last);
		swap(tmp);
	}

	void assign(std::initializer_list<value_type> ilist)
	{
		assign(ilist.begin(), ilist.end());
	}

	// ---------- Element access ---------- //

	[[nodiscard]] reference front()
	{
		RAW_ASSERT(!empty(), "front() called on empty list");
		return static_cast<node*>(m_sentinel.next)->value;
	}

	[[nodiscard]] const_reference front() const
	{
		RAW_ASSERT(!empty(), "front() called on empty list");
		return static_cast<const node*>(m_sentinel.next)->value;
	}

	[[nodiscard]] reference back()
	{
		RAW_ASSERT(!empty(), "back() called on empty list");
		return static_cast<node*>(m_sentinel.prev)->value;
	}

	[[nodiscard]] const_reference back() const
	{
		RAW_ASSERT(!empty(), "back() called on empty list");
		return static_cast<const node*>(m_sentinel.prev)->value;
	}

	// ---------- Iterators ---------- //

	[[nodiscard]] iterator begin() noexcept
	{
		return iterator(m_sentinel.next);
	}

	[[nodiscard]] const_iterator begin() const noexcept
	{
		return const_iterator(m_sentinel.next);
	}

	[[nodiscard]] iterator end() noexcept
	{
		return iterator(&m_sentinel);
	}

	[[nodiscard]] const_iterator end() const noexcept
	{
		return const_iterator(const_cast<node_base*>(&m_sentinel));
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
		return m_sentinel.next == &m_sentinel;
	}

	[[nodiscard]] size_type size() const noexcept
	{
		return m_size;
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
		if (count == 0)
		{
			return iterator(pos.m_ptr);
		}

		auto [head, tail, created] = create_node_chain_n(count, value);
		hook_node_chain(pos.m_ptr, head, tail);
		m_size += created;

		return iterator(head);
	}

	template <std::input_iterator InputIt>
	iterator insert(const_iterator pos, InputIt first, InputIt last)
	{
		if (first == last)
		{
			return iterator(pos.m_ptr);
		}

		auto [head, tail, created] = create_node_chain(first, last);
		hook_node_chain(pos.m_ptr, head, tail);
		m_size += created;

		return iterator(head);
	}

	iterator insert(const_iterator pos, std::initializer_list<value_type> ilist)
	{
		return insert(pos, ilist.begin(), ilist.end());
	}

	template <typename... Args>
	iterator emplace(const_iterator pos, Args&&... args)
	{
		node* new_node = create_node(std::forward<Args>(args)...);
		hook_node(pos.m_ptr, new_node);
		++m_size;
		return iterator(new_node);
	}

	iterator erase(const_iterator pos)
	{
		node_base* next = pos.m_ptr->next;
		node_base* unhooked = unhook_node(pos.m_ptr);
		destroy_node(static_cast<node*>(unhooked));
		--m_size;
		return iterator(next);
	}

	iterator erase(const_iterator first, const_iterator last)
	{
		if (first == last)
		{
			return iterator(last.m_ptr);
		}

		node_base* next = last.m_ptr;
		auto [head, tail] = unhook_node_chain(first.m_ptr, last.m_ptr->prev);
		m_size -= destroy_node_chain(static_cast<node*>(head));

		return iterator(next);
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
		return *emplace(end(), std::forward<Args>(args)...);
	}

	void pop_back()
	{
		RAW_ASSERT(!empty(), "pop_back() called on empty list");
		erase(std::prev(end()));
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
		return *emplace(begin(), std::forward<Args>(args)...);
	}

	void pop_front()
	{
		RAW_ASSERT(!empty(), "pop_front() called on empty list");
		erase(begin());
	}

	void resize(size_type count)
	{
		const size_type old_size = size();

		if (count < old_size)
		{
			iterator it = begin();
			std::advance(it, count);
			erase(it, end());
		}
		else if (count > old_size)
		{
			auto [head, tail, created] = create_node_chain_n(count - old_size);
			hook_node_chain(&m_sentinel, head, tail);
			m_size += created;
		}
	}

	void resize(size_type count, const value_type& value)
	{
		const size_type old_size = size();

		if (count < old_size)
		{
			iterator it = begin();
			std::advance(it, count);
			erase(it, end());
		}
		else if (count > old_size)
		{
			auto [head, tail, created] = create_node_chain_n(count - old_size, value);
			hook_node_chain(&m_sentinel, head, tail);
			m_size += created;
		}
	}

	void swap(list& other) noexcept
	{
		if (this == std::addressof(other))
		{
			return;
		}

		node_base* this_head = m_sentinel.next;
		node_base* this_tail = m_sentinel.prev;
		node_base* other_head = other.m_sentinel.next;
		node_base* other_tail = other.m_sentinel.prev;

		if (other_head != &other.m_sentinel)
		{
			m_sentinel.next = other_head;
			m_sentinel.prev = other_tail;
			other_head->prev = &m_sentinel;
			other_tail->next = &m_sentinel;
		}
		else
		{
			m_sentinel.next = &m_sentinel;
			m_sentinel.prev = &m_sentinel;
		}

		if (this_head != &m_sentinel)
		{
			other.m_sentinel.next = this_head;
			other.m_sentinel.prev = this_tail;
			this_head->prev = &other.m_sentinel;
			this_tail->next = &other.m_sentinel;
		}
		else
		{
			other.m_sentinel.next = &other.m_sentinel;
			other.m_sentinel.prev = &other.m_sentinel;
		}

		using std::swap;
		swap(m_size, other.m_size);
	}

	// ---------- Operations ---------- //

	void merge(list& other)
	{
		merge(other, std::less<>{});
	}

	void merge(list&& other)
	{
		merge(other);
	}

	template <typename Compare>
	void merge(list& other, Compare comp)
	{
		if (this == std::addressof(other) || other.empty())
		{
			return;
		}

		iterator this_it = begin();
		iterator other_it = other.begin();

		while (this_it != end() && other_it != other.end())
		{
			if (comp(*other_it, *this_it))
			{
				iterator other_next = std::next(other_it);
				splice(this_it, other, other_it);
				other_it = other_next;
			}
			else
			{
				++this_it;
			}
		}

		if (other_it != other.end())
		{
			splice(end(), other, other_it, other.end());
		}
	}

	template <typename Compare>
	void merge(list&& other, Compare comp)
	{
		merge(other, comp);
	}

	void splice(const_iterator pos, list& other)
	{
		if (this == std::addressof(other) || other.empty())
		{
			return;
		}

		auto [head, tail] = other.unhook_node_chain(other.m_sentinel.next, other.m_sentinel.prev);
		hook_node_chain(pos.m_ptr, head, tail);

		m_size += other.m_size;
		other.m_size = 0;
	}

	void splice(const_iterator pos, list&& other)
	{
		splice(pos, other);
	}

	void splice(const_iterator pos, list& other, const_iterator it)
	{
		if (pos.m_ptr == it.m_ptr || pos.m_ptr == it.m_ptr->next)
		{
			return;
		}

		node_base* unhooked = other.unhook_node(it.m_ptr);

		if (this != std::addressof(other))
		{
			++m_size;
			--other.m_size;
		}

		hook_node(pos.m_ptr, unhooked);
	}

	void splice(const_iterator pos, list&& other, const_iterator it)
	{
		splice(pos, other, it);
	}

	void splice(const_iterator pos, list& other, const_iterator first, const_iterator last)
	{
		if (first == last)
		{
			return;
		}

		if (this != std::addressof(other))
		{
			size_type count = std::distance(first, last);
			m_size += count;
			other.m_size -= count;
		}

		auto [head, tail] = other.unhook_node_chain(first.m_ptr, last.m_ptr->prev);
		hook_node_chain(pos.m_ptr, head, tail);
	}

	void splice(const_iterator pos, list&& other, const_iterator first, const_iterator last)
	{
		splice(pos, other, first, last);
	}

	size_type remove(const value_type& value)
	{
		return remove_if([&](const value_type& elem) { return elem == value; });
	}

	template <typename UnaryPred>
	size_type remove_if(UnaryPred p)
	{
		iterator it = begin();
		size_type removed = 0;

		while (it != end())
		{
			if (p(*it))
			{
				it = erase(it);
				++removed;
			}
			else
			{
				++it;
			}
		}

		return removed;
	}

	void reverse() noexcept
	{
		if (m_size <= 1)
		{
			return;
		}

		node_base* curr = &m_sentinel;

		do
		{
			using std::swap;
			swap(curr->next, curr->prev);
			curr = curr->prev;
		} while (curr != &m_sentinel);
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

		iterator curr = begin();
		iterator next = std::next(curr);
		size_type removed = 0;

		while (next != end())
		{
			if (p(*curr, *next))
			{
				next = erase(next);
				++removed;
			}
			else
			{
				curr = next;
				++next;
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
		if (m_size <= 1)
		{
			return;
		}

		list carry;
		list bins[64];
		size_type fill = 0;

		while (!empty())
		{
			carry.splice(carry.begin(), *this, begin());

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
	std::tuple<node*, node*, size_type> create_node_chain(InputIt first, InputIt last) const
	{
		if (first == last)
		{
			return { nullptr, nullptr, 0 };
		}

		node* head = create_node(*first);
		node_guard guard(head);

		node* tail = head;
		size_type created = 1;

		for (++first; first != last; ++first, ++created)
		{
			node* new_node = create_node(*first);
			tail->next = new_node;
			new_node->prev = tail;
			tail = new_node;
		}

		guard.release();

		return { head, tail, created };
	}

	std::tuple<node*, node*, size_type> create_node_chain_n(size_type count) const
	{
		if (count == 0)
		{
			return { nullptr, nullptr, 0 };
		}

		node* head = create_node();
		node_guard guard(head);

		node* tail = head;
		size_type created = 1;

		for (--count; count > 0; --count, ++created)
		{
			node* new_node = create_node();
			tail->next = new_node;
			new_node->prev = tail;
			tail = new_node;
		}

		guard.release();

		return { head, tail, created };
	}

	std::tuple<node*, node*, size_type> create_node_chain_n(size_type count, const value_type& value) const
	{
		if (count == 0)
		{
			return { nullptr, nullptr, 0 };
		}

		node* head = create_node(value);
		node_guard guard(head);

		node* tail = head;
		size_type created = 1;

		for (--count; count > 0; --count, ++created)
		{
			node* new_node = create_node(value);
			tail->next = new_node;
			new_node->prev = tail;
			tail = new_node;
		}

		guard.release();

		return { head, tail, created };
	}

	void destroy_node(node* node_to_destroy) const
	{
		std::destroy_at(node_to_destroy);
		detail::deallocate(node_to_destroy, 1);
	}

	size_type destroy_node_chain(node* head) const
	{
		size_type destroyed = 0;

		while (head && head != &m_sentinel)
		{
			node* next = static_cast<node*>(head->next);
			destroy_node(head);
			head = next;
			++destroyed;
		}

		return destroyed;
	}

	void hook_node(node_base* pos, node_base* node_to_hook) const
	{
		node_to_hook->next = pos;
		node_to_hook->prev = pos->prev;
		pos->prev->next = node_to_hook;
		pos->prev = node_to_hook;
	}

	void hook_node_chain(node_base* pos, node_base* head, node_base* tail) const
	{
		head->prev = pos->prev;
		tail->next = pos;
		pos->prev->next = head;
		pos->prev = tail;
	}

	node_base* unhook_node(node_base* node_to_unhook) const
	{
		node_to_unhook->prev->next = node_to_unhook->next;
		node_to_unhook->next->prev = node_to_unhook->prev;
		node_to_unhook->next = nullptr;
		node_to_unhook->prev = nullptr;
		return node_to_unhook;
	}

	std::pair<node_base*, node_base*> unhook_node_chain(node_base* head, node_base* tail) const
	{
		head->prev->next = tail->next;
		tail->next->prev = head->prev;
		head->prev = nullptr;
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

		auto [head, tail, created] = create_node_chain(first, last);
		change_chain(head, tail, created);
	}

	void construct_n(size_type count)
	{
		if (count == 0)
		{
			return;
		}

		auto [head, tail, created] = create_node_chain_n(count);
		change_chain(head, tail, created);
	}

	void construct_n(size_type count, const value_type& value)
	{
		if (count == 0)
		{
			return;
		}

		auto [head, tail, created] = create_node_chain_n(count, value);
		change_chain(head, tail, created);
	}

	void change_chain(node_base* new_head, node_base* new_tail, size_type new_size)
	{
		if (m_sentinel.next != &m_sentinel)
		{
			destroy_node_chain(static_cast<node*>(m_sentinel.next));
		}

		if (new_head)
		{
			m_sentinel.next = new_head;
			m_sentinel.prev = new_tail;
			new_head->prev = &m_sentinel;
			new_tail->next = &m_sentinel;
		}
		else
		{
			m_sentinel.next = &m_sentinel;
			m_sentinel.prev = &m_sentinel;
		}

		m_size = new_size;
	}

	void tidy()
	{
		change_chain(&m_sentinel, &m_sentinel, 0);
	}

	// ---------- Variables ---------- //

	node_base m_sentinel;
	size_type m_size;
};

// ---------- Non-member functions ---------- //

template <typename T>
[[nodiscard]] bool operator==(const list<T>& lhs, const list<T>& rhs)
{
	return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <typename T>
[[nodiscard]] auto operator<=>(const list<T>& lhs, const list<T>& rhs)
{
	return std::lexicographical_compare_three_way(
		lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), std::compare_three_way{});
}

template <typename T>
void swap(list<T>& lhs, list<T>& rhs) noexcept(noexcept(lhs.swap(rhs)))
{
	lhs.swap(rhs);
}

template <typename T, typename U>
typename list<T>::size_type erase(list<T>& c, const U& value)
{
	return c.remove_if([&](const auto& elem) -> bool { return elem == value; });
}

template <typename T, typename Pred>
typename list<T>::size_type erase_if(list<T>& c, Pred pred)
{
	return c.remove_if(pred);
}

// ---------- Deduction guides ---------- //

template <std::input_iterator InputIt>
list(InputIt, InputIt) -> list<std::iter_value_t<InputIt>>;

} // namespace raw
