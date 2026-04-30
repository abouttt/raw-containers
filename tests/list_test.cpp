#include <gtest/gtest.h>
#include <raw/list.h>

#include <algorithm>
#include <compare>
#include <initializer_list>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

template <typename T>
bool equal(const raw::list<T>& l, std::initializer_list<T> ilist)
{
	if (l.size() != ilist.size())
	{
		return false;
	}

	return std::equal(l.begin(), l.end(), ilist.begin());
}

TEST(ListTest, Constructors)
{
	// Default constructor
	raw::list<int> l1;
	EXPECT_TRUE(l1.empty());
	EXPECT_EQ(l1.size(), 0);

	// Size constructor (default-inserted)
	raw::list<int> l2(5);
	EXPECT_EQ(l2.size(), 5);
	for (int x : l2)
	{
		EXPECT_EQ(x, 0);
	}

	// Size + value constructor
	raw::list<int> l3(3, 42);
	EXPECT_TRUE(equal(l3, { 42, 42, 42 }));

	// Range constructor (forward iterator)
	std::vector<int> src = { 1, 2, 3, 4 };
	raw::list<int> l4(src.begin(), src.end());
	EXPECT_TRUE(equal(l4, { 1, 2, 3, 4 }));

	// Range constructor (input iterator via stringstream)
	std::istringstream iss("100 200 300");
	std::istream_iterator<int> iit(iss), iend;
	raw::list<int> l5(iit, iend);
	EXPECT_TRUE(equal(l5, { 100, 200, 300 }));

	// Initializer list constructor
	raw::list<int> l6 = { 5, 6, 7 };
	EXPECT_TRUE(equal(l6, { 5, 6, 7 }));

	// Copy constructor
	raw::list<int> l7(l6);
	EXPECT_TRUE(equal(l7, { 5, 6, 7 }));

	// Move constructor
	raw::list<int> l8(std::move(l7));
	EXPECT_TRUE(equal(l8, { 5, 6, 7 }));
	EXPECT_TRUE(l7.empty()); // moved-from list is valid but unspecified; usually empty
}

TEST(ListTest, Assignment)
{
	raw::list<int> l1 = { 1, 2, 3 };
	raw::list<int> l2 = { 4, 5 };

	// Copy assignment
	l1 = l2;
	EXPECT_TRUE(equal(l1, { 4, 5 }));
	EXPECT_EQ(l1.size(), 2);

	// Move assignment
	raw::list<int> l3 = { 6, 7, 8 };
	l1 = std::move(l3);
	EXPECT_TRUE(equal(l1, { 6, 7, 8 }));
	EXPECT_TRUE(l3.empty());

	// Initializer list assignment
	l1 = { 9, 10 };
	EXPECT_TRUE(equal(l1, { 9, 10 }));

	// Self assignment
	l1 = l1;
	EXPECT_TRUE(equal(l1, { 9, 10 }));

	// assign(size, value)
	l1.assign(3, 42);
	EXPECT_TRUE(equal(l1, { 42, 42, 42 }));

	// assign(range)
	std::vector<int> vec = { 1, 2, 3 };
	l1.assign(vec.begin(), vec.end());
	EXPECT_TRUE(equal(l1, { 1, 2, 3 }));

	// assign(initializer_list)
	l1.assign({ 7, 8, 9 });
	EXPECT_TRUE(equal(l1, { 7, 8, 9 }));
}

TEST(ListTest, ElementAccess)
{
	raw::list<int> l = { 10, 20, 30 };

	EXPECT_EQ(l.front(), 10);
	EXPECT_EQ(l.back(), 30);
	l.front() = 1;
	l.back() = 100;
	EXPECT_EQ(l.front(), 1);
	EXPECT_EQ(l.back(), 100);

	const raw::list<int>& cl = l;
	EXPECT_EQ(cl.front(), 1);
	EXPECT_EQ(cl.back(), 100);
}

TEST(ListTest, Iterators)
{
	raw::list<int> l = { 1, 2, 3, 4, 5 };

	// Forward iteration
	int sum = 0;
	for (auto it = l.begin(); it != l.end(); ++it)
	{
		sum += *it;
	}
	EXPECT_EQ(sum, 15);

	// Reverse iteration
	int rsum = 0;
	for (auto it = l.rbegin(); it != l.rend(); ++it)
	{
		rsum += *it;
	}
	EXPECT_EQ(rsum, 15);

	// Const iterators
	const raw::list<int>& cl = l;
	int csum = 0;
	for (auto it = cl.cbegin(); it != cl.cend(); ++it)
	{
		csum += *it;
	}
	EXPECT_EQ(csum, 15);

	// Decrement
	auto it = l.end();
	--it;
	EXPECT_EQ(*it, 5);

	// Empty list iterators
	raw::list<int> empty;
	EXPECT_EQ(empty.begin(), empty.end());
}

TEST(ListTest, Capacity)
{
	raw::list<int> l;
	EXPECT_TRUE(l.empty());
	EXPECT_EQ(l.size(), 0);

	l.push_back(1);
	EXPECT_FALSE(l.empty());
	EXPECT_EQ(l.size(), 1);

	// max_size is huge (typically)
	EXPECT_GT(l.max_size(), 1000000);
}

TEST(ListTest, Modifiers)
{
	raw::list<int> l;

	// push_back / pop_back
	l.push_back(1);
	l.push_back(2);
	l.emplace_back(3);
	EXPECT_TRUE(equal(l, { 1, 2, 3 }));
	l.pop_back();
	EXPECT_TRUE(equal(l, { 1, 2 }));
	l.pop_back();
	l.pop_back();
	EXPECT_TRUE(l.empty());

	// push_front / pop_front
	l.push_front(10);
	l.push_front(20);
	l.emplace_front(30);
	EXPECT_TRUE(equal(l, { 30, 20, 10 }));
	l.pop_front();
	EXPECT_TRUE(equal(l, { 20, 10 }));

	// clear
	l = { 5, 6, 7 };
	l.clear();
	EXPECT_TRUE(l.empty());

	// insert single element (lvalue)
	l = { 1, 2, 3 };
	auto it = l.insert(std::next(l.begin()), 99);
	EXPECT_EQ(*it, 99);
	EXPECT_TRUE(equal(l, { 1, 99, 2, 3 }));

	// insert rvalue
	l.insert(l.begin(), 100);
	EXPECT_TRUE(equal(l, { 100, 1, 99, 2, 3 }));

	// insert count copies
	l = { 1, 2, 3 };
	l.insert(std::next(l.begin()), 2, 42);
	EXPECT_TRUE(equal(l, { 1, 42, 42, 2, 3 }));

	// insert range (forward iterator)
	l = { 1, 2, 3 };
	std::vector<int> src = { 10, 20 };
	l.insert(std::next(l.begin(), 2), src.begin(), src.end());
	EXPECT_TRUE(equal(l, { 1, 2, 10, 20, 3 }));

	// insert range (input iterator)
	std::istringstream iss("100 200");
	std::istream_iterator<int> iit(iss), iend;
	l.insert(std::prev(l.end()), iit, iend);
	EXPECT_TRUE(equal(l, { 1, 2, 10, 20, 100, 200, 3 }));

	// insert initializer_list
	l = { 1, 2, 3 };
	l.insert(std::next(l.begin()), { 4, 5 });
	EXPECT_TRUE(equal(l, { 1, 4, 5, 2, 3 }));

	// emplace
	l = { 1, 2, 3 };
	it = l.emplace(std::next(l.begin()), 99);
	EXPECT_EQ(*it, 99);
	EXPECT_TRUE(equal(l, { 1, 99, 2, 3 }));

	// erase single
	l.erase(std::next(l.begin())); // removes 99
	EXPECT_TRUE(equal(l, { 1, 2, 3 }));

	// erase range
	l.erase(std::next(l.begin()), std::prev(l.end())); // removes 2
	EXPECT_TRUE(equal(l, { 1, 3 }));

	// resize (default)
	l.resize(5);
	EXPECT_EQ(l.size(), 5);
	auto it2 = l.begin();
	EXPECT_EQ(*it2++, 1);
	EXPECT_EQ(*it2++, 3);
	while (it2 != l.end())
	{
		EXPECT_EQ(*it2, 0);
		++it2;
	}
	l.resize(2);
	EXPECT_TRUE(equal(l, { 1, 3 }));

	// resize (with value)
	l.resize(4, 42);
	EXPECT_TRUE(equal(l, { 1, 3, 42, 42 }));
}

TEST(ListTest, Operations)
{
	// merge
	raw::list<int> l1 = { 1, 3, 5 };
	raw::list<int> l2 = { 2, 4, 6 };
	l1.merge(l2);
	EXPECT_TRUE(equal(l1, { 1, 2, 3, 4, 5, 6 }));
	EXPECT_TRUE(l2.empty());

	l1 = { 5, 3, 1 };
	l2 = { 6, 4, 2 };
	l1.merge(l2, std::greater<>{});
	EXPECT_TRUE(equal(l1, { 6, 5, 4, 3, 2, 1 }));
	EXPECT_TRUE(l2.empty());

	// splice (whole list)
	l1 = { 1, 2, 3 };
	l2 = { 4, 5, 6 };
	l1.splice(std::next(l1.begin()), l2);
	EXPECT_TRUE(equal(l1, { 1, 4, 5, 6, 2, 3 }));
	EXPECT_TRUE(l2.empty());

	// splice (single element)
	l1 = { 1, 2, 3 };
	l2 = { 4, 5, 6 };
	l1.splice(l1.end(), l2, std::next(l2.begin()));
	EXPECT_TRUE(equal(l1, { 1, 2, 3, 5 }));
	EXPECT_TRUE(equal(l2, { 4, 6 }));

	// splice (range)
	l1 = { 1, 2, 3 };
	l2 = { 4, 5, 6, 7 };
	l1.splice(std::next(l1.begin()), l2, std::next(l2.begin()), std::prev(l2.end()));
	EXPECT_TRUE(equal(l1, { 1, 5, 6, 2, 3 }));
	EXPECT_TRUE(equal(l2, { 4, 7 }));

	// splice (self)
	l1 = { 1, 2, 3, 4 };
	l1.splice(l1.end(), l1, l1.begin());
	EXPECT_TRUE(equal(l1, { 2, 3, 4, 1 }));

	// remove
	l1 = { 1, 2, 3, 2, 4, 2, 5 };
	auto cnt = l1.remove(2);
	EXPECT_EQ(cnt, 3);
	EXPECT_TRUE(equal(l1, { 1, 3, 4, 5 }));

	// remove_if
	cnt = l1.remove_if([](int x) { return x % 2 == 0; });
	EXPECT_EQ(cnt, 1);
	EXPECT_TRUE(equal(l1, { 1, 3, 5 }));

	// reverse
	l1 = { 1, 2, 3, 4, 5 };
	l1.reverse();
	EXPECT_TRUE(equal(l1, { 5, 4, 3, 2, 1 }));

	// unique
	l1 = { 1, 1, 2, 2, 2, 3, 3, 1, 1 };
	cnt = l1.unique();
	EXPECT_EQ(cnt, 5);
	EXPECT_TRUE(equal(l1, { 1, 2, 3, 1 }));

	// sort (ascending)
	l1 = { 3, 1, 4, 1, 5, 9, 2, 6 };
	l1.sort();
	EXPECT_TRUE(equal(l1, { 1, 1, 2, 3, 4, 5, 6, 9 }));

	// sort (descending)
	l1.sort(std::greater<>{});
	EXPECT_TRUE(equal(l1, { 9, 6, 5, 4, 3, 2, 1, 1 }));
}

TEST(ListTest, Comparisons)
{
	raw::list<int> l1 = { 1, 2, 3 };
	raw::list<int> l2 = { 1, 2, 3 };
	raw::list<int> l3 = { 7, 8, 9, 10 };

	EXPECT_FALSE(l1 != l2);
	EXPECT_TRUE(l1 == l2);
	EXPECT_FALSE(l1 < l2);
	EXPECT_TRUE(l1 <= l2);
	EXPECT_FALSE(l1 > l2);
	EXPECT_TRUE(l1 >= l2);
	EXPECT_TRUE(l1 <=> l2 != std::weak_ordering::less);
	EXPECT_TRUE(l1 <=> l2 != std::weak_ordering::greater);
	EXPECT_TRUE(l1 <=> l2 == std::weak_ordering::equivalent);
	EXPECT_TRUE(l1 <=> l2 >= 0);
	EXPECT_TRUE(l1 <=> l2 <= 0);
	EXPECT_TRUE(l1 <=> l2 == 0);

	EXPECT_TRUE(l1 != l3);
	EXPECT_FALSE(l1 == l3);
	EXPECT_TRUE(l1 < l3);
	EXPECT_TRUE(l1 <= l3);
	EXPECT_FALSE(l1 > l3);
	EXPECT_FALSE(l1 >= l3);
	EXPECT_TRUE(l1 <=> l3 == std::weak_ordering::less);
	EXPECT_TRUE(l1 <=> l3 != std::weak_ordering::greater);
	EXPECT_TRUE(l1 <=> l3 != std::weak_ordering::equivalent);
	EXPECT_TRUE(l1 <=> l3 < 0);
	EXPECT_TRUE(l1 <=> l3 != 0);
	EXPECT_TRUE(l1 <=> l3 <= 0);
}

TEST(ListTest, NonMemberSwap)
{
	raw::list<int> l1 = { 1, 2, 3 };
	raw::list<int> l2 = { 4, 5, 6 };
	swap(l1, l2);
	EXPECT_TRUE(equal(l1, { 4, 5, 6 }));
	EXPECT_TRUE(equal(l2, { 1, 2, 3 }));
}

TEST(ListTest, NonMemberErase)
{
	raw::list<int> l = { 1, 2, 3, 2, 4, 2, 5 };
	auto cnt = erase(l, 2);
	EXPECT_EQ(cnt, 3);
	EXPECT_TRUE(equal(l, { 1, 3, 4, 5 }));

	l = { 1, 2, 3, 4, 5, 6 };
	cnt = erase_if(l, [](int x) { return x % 2 == 0; });
	EXPECT_EQ(cnt, 3);
	EXPECT_TRUE(equal(l, { 1, 3, 5 }));
}

TEST(ListTest, MoveOnly)
{
	struct MoveOnly
	{
		int val;

		explicit MoveOnly(int v)
			: val(v)
		{
		}

		MoveOnly(MoveOnly&& o) noexcept
			: val(o.val)
		{
			o.val = 0;
		}

		MoveOnly(const MoveOnly&) = delete;

		bool operator==(const MoveOnly& rhs) const
		{
			return val == rhs.val;
		}
	};

	raw::list<MoveOnly> l;
	l.emplace_back(1);
	l.push_back(MoveOnly(2));
	EXPECT_EQ(l.size(), 2);
	EXPECT_EQ(l.front().val, 1);
	EXPECT_EQ(l.back().val, 2);

	raw::list<MoveOnly> l2;
	l2.emplace_back(3);
	l.splice(l.end(), l2);
	EXPECT_EQ(l.size(), 3);
	EXPECT_TRUE(l2.empty());
}
