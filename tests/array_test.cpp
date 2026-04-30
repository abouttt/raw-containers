#include <gtest/gtest.h>
#include <raw/array.h>

#include <algorithm>
#include <compare>
#include <initializer_list>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <typename T, std::size_t N>
bool equal(const raw::array<T, N>& a, std::initializer_list<T> ilist)
{
	if (a.size() != ilist.size())
	{
		return false;
	}

	return std::equal(a.begin(), a.end(), ilist.begin());
}

TEST(ArrayTest, Constructors)
{
	// Aggregate initialization
	raw::array<int, 3> arr1 = { 1, 2, 3 };
	EXPECT_EQ(arr1[0], 1);
	EXPECT_EQ(arr1[1], 2);
	EXPECT_EQ(arr1[2], 3);

	// CTAD (Class Template Argument Deduction)
	raw::array arr2 = { 4, 5, 6, 7 };
	static_assert(std::is_same_v<decltype(arr2), raw::array<int, 4>>);
	EXPECT_EQ(arr2.size(), 4);
	EXPECT_EQ(arr2[3], 7);

	// Zero-sized array initialization
	raw::array<int, 0> empty_arr{};
	EXPECT_TRUE(empty_arr.empty());
	EXPECT_EQ(empty_arr.size(), 0);

	// Partial initialization (remaining elements zero-initialized)
	raw::array<int, 5> partial = { 1, 2 };
	EXPECT_EQ(partial[2], 0);
	EXPECT_EQ(partial[4], 0);

	// Constexpr construction
	constexpr raw::array<int, 3> carr = { 10, 20, 30 };
	static_assert(carr[0] == 10);
}

TEST(ArrayTest, Assignment)
{
	raw::array<int, 3> arr1 = { 1, 2, 3 };
	raw::array<int, 3> arr2 = { 4, 5, 6 };

	// Copy assignment
	arr1 = arr2;
	EXPECT_EQ(arr1[0], 4);
	EXPECT_EQ(arr1[1], 5);
	EXPECT_EQ(arr1[2], 6);
	// arr2 unchanged
	EXPECT_EQ(arr2[0], 4);
	EXPECT_EQ(arr2[1], 5);
	EXPECT_EQ(arr2[2], 6);

	// Move assignment
	raw::array<int, 3> arr3 = { 7, 8, 9 };
	arr1 = std::move(arr3);  // arr3 is valid but unspecified
	EXPECT_EQ(arr1[0], 7);
	EXPECT_EQ(arr1[1], 8);
	EXPECT_EQ(arr1[2], 9);

	// Self assignment
	arr1 = arr1;
	EXPECT_EQ(arr1[0], 7);
}

TEST(ArrayTest, ElementAccess)
{
	raw::array<int, 4> arr = { 10, 20, 30, 40 };

	// at (within bounds)
	EXPECT_EQ(arr.at(2), 30);
	EXPECT_EQ(arr[1], 20);

	// at out of bounds
	EXPECT_THROW(arr.at(4), std::out_of_range);
	const auto& carr = arr;
	EXPECT_THROW(carr.at(4), std::out_of_range);

	// front / back
	EXPECT_EQ(arr.front(), 10);
	EXPECT_EQ(arr.back(), 40);
	arr.front() = 1;
	arr.back() = 100;
	EXPECT_EQ(arr.front(), 1);
	EXPECT_EQ(arr.back(), 100);

	// const versions
	const raw::array<int, 4>& cr = arr;
	EXPECT_EQ(cr.front(), 1);
	EXPECT_EQ(cr.back(), 100);
	EXPECT_EQ(cr[2], 30);
	EXPECT_EQ(cr.at(2), 30);

	// data()
	int* p = arr.data();
	EXPECT_EQ(*p, 1);
	const int* cp = cr.data();
	EXPECT_EQ(cp[2], 30);

	// Zero‑sized array element access (UB must be avoided when testing)
	raw::array<int, 0> zero;
	EXPECT_THROW(zero.at(0), std::out_of_range);
	// operator[], front, back on zero size would be UB; we skip testing them.
}

TEST(ArrayTest, Iterators)
{
	raw::array<int, 3> arr = { 1, 2, 3 };

	// Forward iteration
	int sum = 0;
	for (auto it = arr.begin(); it != arr.end(); ++it)
	{
		sum += *it;
	}
	EXPECT_EQ(sum, 6);

	// Reverse iteration
	int rsum = 0;
	for (auto it = arr.rbegin(); it != arr.rend(); ++it)
	{
		rsum += *it;
	}
	EXPECT_EQ(rsum, 6);

	// Const iterators
	const raw::array<int, 3>& carr = arr;
	static_assert(std::is_same_v<decltype(carr.begin()), raw::array<int, 3>::const_iterator>);
	EXPECT_EQ(*carr.cbegin(), 1);
	EXPECT_EQ(*carr.crbegin(), 3);

	// Iterator distance
	EXPECT_EQ(arr.end() - arr.begin(), 3);

	// Zero sized array iterators
	raw::array<int, 0> empty;
	EXPECT_EQ(empty.begin(), empty.end());
}

TEST(ArrayTest, Capacity)
{
	raw::array<int, 5> arr = { 1, 2, 3, 4, 5 };
	EXPECT_EQ(arr.size(), 5);
	EXPECT_EQ(arr.max_size(), 5);
	EXPECT_FALSE(arr.empty());

	raw::array<int, 0> zero;
	EXPECT_EQ(zero.size(), 0);
	EXPECT_EQ(zero.max_size(), 0);
	EXPECT_TRUE(zero.empty());
}

TEST(ArrayTest, Operations)
{
	// fill
	raw::array<int, 3> arr1{};
	arr1.fill(9);
	EXPECT_TRUE(equal(arr1, { 9, 9, 9 }));

	// swap member
	raw::array<int, 3> arr2 = { 0, 0, 0 };
	arr1.swap(arr2);
	EXPECT_TRUE(equal(arr1, { 0, 0, 0 }));
	EXPECT_TRUE(equal(arr2, { 9, 9, 9 }));

	// self-swap (should be safe)
	arr1.swap(arr1);
	EXPECT_TRUE(equal(arr1, { 0, 0, 0 }));
}

TEST(ArrayTest, Comparisons)
{
	raw::array<int, 3> arr1 = { 1, 2, 3 };
	raw::array<int, 3> arr2 = { 1, 2, 3 };
	raw::array<int, 3> arr3 = { 7, 8, 9 };

	EXPECT_FALSE(arr1 != arr2);
	EXPECT_TRUE(arr1 == arr2);
	EXPECT_FALSE(arr1 < arr2);
	EXPECT_TRUE(arr1 <= arr2);
	EXPECT_FALSE(arr1 > arr2);
	EXPECT_TRUE(arr1 >= arr2);
	EXPECT_TRUE(arr1 <=> arr2 != std::weak_ordering::less);
	EXPECT_TRUE(arr1 <=> arr2 != std::weak_ordering::greater);
	EXPECT_TRUE(arr1 <=> arr2 == std::weak_ordering::equivalent);
	EXPECT_TRUE(arr1 <=> arr2 >= 0);
	EXPECT_TRUE(arr1 <=> arr2 <= 0);
	EXPECT_TRUE(arr1 <=> arr2 == 0);

	EXPECT_TRUE(arr1 != arr3);
	EXPECT_FALSE(arr1 == arr3);
	EXPECT_TRUE(arr1 < arr3);
	EXPECT_TRUE(arr1 <= arr3);
	EXPECT_FALSE(arr1 > arr3);
	EXPECT_FALSE(arr1 >= arr3);
	EXPECT_TRUE(arr1 <=> arr3 == std::weak_ordering::less);
	EXPECT_TRUE(arr1 <=> arr3 != std::weak_ordering::greater);
	EXPECT_TRUE(arr1 <=> arr3 != std::weak_ordering::equivalent);
	EXPECT_TRUE(arr1 <=> arr3 < 0);
	EXPECT_TRUE(arr1 <=> arr3 != 0);
	EXPECT_TRUE(arr1 <=> arr3 <= 0);
}

TEST(ArrayTest, NonMemberSwap)
{
	raw::array<int, 3> a1 = { 1, 2, 3 };
	raw::array<int, 3> a2 = { 4, 5, 6 };
	swap(a1, a2);
	EXPECT_TRUE(equal(a1, { 4, 5, 6 }));
	EXPECT_TRUE(equal(a2, { 1, 2, 3 }));
}

TEST(ArrayTest, TupleInterface)
{
	raw::array<int, 2> arr = { 100, 200 };
	static_assert(std::tuple_size_v<decltype(arr)> == 2);
	static_assert(std::is_same_v<std::tuple_element_t<0, decltype(arr)>, int>);
	static_assert(std::is_same_v<std::tuple_element_t<1, decltype(arr)>, int>);

	EXPECT_EQ(raw::get<0>(arr), 100);
	EXPECT_EQ(raw::get<1>(arr), 200);

	// const get
	const auto& carr = arr;
	EXPECT_EQ(raw::get<0>(carr), 100);

	// rvalue get
	struct MoveTracker
	{
		int val;
		bool moved = false;

		MoveTracker(int v)
			: val(v)
		{
		}

		MoveTracker(MoveTracker&& o) noexcept
			: val(o.val)
			, moved(true)
		{
			o.val = 0;
		}

		MoveTracker(const MoveTracker&) = delete;
	};

	raw::array<MoveTracker, 1> mv = { MoveTracker(42) };
	MoveTracker m = raw::get<0>(std::move(mv));
	EXPECT_TRUE(m.moved);
	EXPECT_EQ(m.val, 42);
}

TEST(ArrayTest, ToArray)
{
	int c_arr[] = { 1, 2, 3 };
	auto arr1 = raw::to_array(c_arr);
	static_assert(std::is_same_v<decltype(arr1), raw::array<int, 3>>);
	EXPECT_TRUE(equal(arr1, { 1, 2, 3 }));

	// move from temporary array
	auto arr2 = raw::to_array({ 4, 5, 6 });   // calls rvalue overload
	static_assert(std::is_same_v<decltype(arr2), raw::array<int, 3>>);
	EXPECT_TRUE(equal(arr2, { 4, 5, 6 }));
}

TEST(ArrayTest, StructuredBinding)
{
	raw::array<int, 3> arr = { 10, 20, 30 };
	auto [a, b, c] = arr;
	EXPECT_EQ(a, 10);
	EXPECT_EQ(b, 20);
	EXPECT_EQ(c, 30);
}

TEST(ArrayTest, ConstexprSupport)
{
	constexpr raw::array<int, 3> arr = { 1, 2, 3 };
	constexpr int sum = arr[0] + arr[1] + arr[2];
	static_assert(sum == 6);
	static_assert(arr.size() == 3);
	static_assert(arr.max_size() == 3);
	static_assert(!arr.empty());

	// fill in constexpr
	constexpr auto filled = []
		{
			raw::array<int, 3> a{};
			a.fill(5);
			return a;
		}();
	static_assert(filled[0] == 5 && filled[2] == 5);

	// swap in constexpr
	constexpr auto swapped = []
		{
			raw::array<int, 2> a = { 1, 2 };
			raw::array<int, 2> b = { 3, 4 };
			a.swap(b);
			return a;
		}();
	static_assert(swapped[0] == 3 && swapped[1] == 4);
}

TEST(ArrayTest, ZeroSizeBehavior)
{
	raw::array<int, 0> z;
	EXPECT_TRUE(z.empty());
	EXPECT_EQ(z.size(), 0);
	EXPECT_EQ(z.max_size(), 0);

	// data() returns nullptr
	EXPECT_EQ(z.data(), nullptr);
	const auto& cz = z;
	EXPECT_EQ(cz.data(), nullptr);

	// fill / swap are no‑ops
	z.fill(42);
	z.swap(z);

	// iterators: begin == end
	EXPECT_EQ(z.begin(), z.end());
	EXPECT_EQ(z.rbegin(), z.rend());
}
