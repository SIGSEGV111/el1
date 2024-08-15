#include <gtest/gtest.h>
#include <el1/math.hpp>

namespace el1::math
{
	TEST(math_vector, DefaultConstructor) {
		vector_t<int, 3> v;
		EXPECT_EQ(v[0], 0);
		EXPECT_EQ(v[1], 0);
		EXPECT_EQ(v[2], 0);
	}

	TEST(math_vector, ArrayConstructor) {
		int arr[3] = {1, 2, 3};
		vector_t<int, 3> v(arr);
		EXPECT_EQ(v[0], 1);
		EXPECT_EQ(v[1], 2);
		EXPECT_EQ(v[2], 3);
	}

	TEST(math_vector, VariadicConstructor) {
		vector_t<int, 3> v(4, 5, 6);
		EXPECT_EQ(v[0], 4);
		EXPECT_EQ(v[1], 5);
		EXPECT_EQ(v[2], 6);
	}

	// Element access tests
	TEST(math_vector, ElementAccessNonConst) {
		vector_t<int, 3> v(7, 8, 9);
		EXPECT_EQ(v[0], 7);
		EXPECT_EQ(v[1], 8);
		EXPECT_EQ(v[2], 9);

		v[1] = 10;
		EXPECT_EQ(v[1], 10);
	}

	TEST(math_vector, ElementAccessConst) {
		const vector_t<int, 3> v(7, 8, 9);
		EXPECT_EQ(v[0], 7);
		EXPECT_EQ(v[1], 8);
		EXPECT_EQ(v[2], 9);
	}

	// Arithmetic operation tests
	TEST(math_vector, VectorAddition) {
		vector_t<int, 3> v1(1, 2, 3);
		vector_t<int, 3> v2(4, 5, 6);
		vector_t<int, 3> v3 = v1 + v2;

		EXPECT_EQ(v3[0], 5);
		EXPECT_EQ(v3[1], 7);
		EXPECT_EQ(v3[2], 9);
	}

	TEST(math_vector, VectorSubtraction) {
		vector_t<int, 3> v1(5, 6, 7);
		vector_t<int, 3> v2(1, 2, 3);
		vector_t<int, 3> v3 = v1 - v2;

		EXPECT_EQ(v3[0], 4);
		EXPECT_EQ(v3[1], 4);
		EXPECT_EQ(v3[2], 4);
	}

	TEST(math_vector, VectorMultiplicationByScalar) {
		vector_t<int, 3> v(1, 2, 3);
		vector_t<int, 3> v2 = v * 2;

		EXPECT_EQ(v2[0], 2);
		EXPECT_EQ(v2[1], 4);
		EXPECT_EQ(v2[2], 6);
	}

	TEST(math_vector, VectorDivisionByScalar) {
		vector_t<int, 3> v(2, 4, 6);
		vector_t<int, 3> v2 = v / 2;

		EXPECT_EQ(v2[0], 1);
		EXPECT_EQ(v2[1], 2);
		EXPECT_EQ(v2[2], 3);
	}

	// Comparison operation tests
	TEST(math_vector, AllBigger) {
		vector_t<int, 3> v1(5, 6, 7);
		vector_t<int, 3> v2(1, 2, 3);
		EXPECT_TRUE(v1.AllBigger(v2));
		EXPECT_FALSE(v2.AllBigger(v1));
	}

	TEST(math_vector, AllLess) {
		vector_t<int, 3> v1(1, 2, 3);
		vector_t<int, 3> v2(5, 6, 7);
		EXPECT_TRUE(v1.AllLess(v2));
		EXPECT_FALSE(v2.AllLess(v1));
	}

	TEST(math_vector, AllBiggerEqual) {
		vector_t<int, 3> v1(5, 6, 7);
		vector_t<int, 3> v2(5, 5, 7);
		EXPECT_TRUE(v1.AllBiggerEqual(v2));
		EXPECT_FALSE(v2.AllBiggerEqual(v1));
	}

	TEST(math_vector, AllLessEqual) {
		vector_t<int, 3> v1(1, 2, 3);
		vector_t<int, 3> v2(1, 3, 3);
		EXPECT_TRUE(v1.AllLessEqual(v2));
		EXPECT_FALSE(v2.AllLessEqual(v1));
	}

	TEST(math_vector, Equality) {
		vector_t<int, 3> v1(1, 2, 3);
		vector_t<int, 3> v2(1, 2, 3);
		vector_t<int, 3> v3(3, 2, 1);

		EXPECT_TRUE(v1 == v2);
		EXPECT_FALSE(v1 == v3);
	}

	// Utility function tests
	TEST(math_vector, Space) {
		vector_t<int, 2> v1(3, 4);
		EXPECT_EQ(v1.Space(), 12);

		vector_t<int, 3> v2(2, 3, 4);
		EXPECT_EQ(v2.Space(), 24);
	}

	// Type conversion tests
	TEST(math_vector, TypeConversion) {
		vector_t<int, 3> v1(1, 2, 3);
		vector_t<double, 3> v2 = (vector_t<double, 3>)v1;

		EXPECT_DOUBLE_EQ(v2[0], 1.0);
		EXPECT_DOUBLE_EQ(v2[1], 2.0);
		EXPECT_DOUBLE_EQ(v2[2], 3.0);
	}
}
