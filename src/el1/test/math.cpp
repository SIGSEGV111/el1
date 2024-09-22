#include <gtest/gtest.h>
#include <el1/math.hpp>
#include <el1/math_vector.hpp>
#include <el1/math_matrix.hpp>

namespace el1::math
{
	using namespace math::vector;
	using namespace math::matrix;

	TEST(math_vector, DefaultConstructor) {
		TVector<int, 3> v;
		EXPECT_EQ(v[0], 0);
		EXPECT_EQ(v[1], 0);
		EXPECT_EQ(v[2], 0);
	}

	TEST(math_vector, ArrayConstructor) {
		int arr[3] = {1, 2, 3};
		TVector<int, 3> v(arr);
		EXPECT_EQ(v[0], 1);
		EXPECT_EQ(v[1], 2);
		EXPECT_EQ(v[2], 3);
	}

	TEST(math_vector, VariadicConstructor) {
		TVector<int, 3> v(4, 5, 6);
		EXPECT_EQ(v[0], 4);
		EXPECT_EQ(v[1], 5);
		EXPECT_EQ(v[2], 6);
	}

	// Element access tests
	TEST(math_vector, ElementAccessNonConst) {
		TVector<int, 3> v(7, 8, 9);
		EXPECT_EQ(v[0], 7);
		EXPECT_EQ(v[1], 8);
		EXPECT_EQ(v[2], 9);

		v[1] = 10;
		EXPECT_EQ(v[1], 10);
	}

	TEST(math_vector, ElementAccessConst) {
		const TVector<int, 3> v(7, 8, 9);
		EXPECT_EQ(v[0], 7);
		EXPECT_EQ(v[1], 8);
		EXPECT_EQ(v[2], 9);
	}

	// Arithmetic operation tests
	TEST(math_vector, VectorAddition) {
		TVector<int, 3> v1(1, 2, 3);
		TVector<int, 3> v2(4, 5, 6);
		TVector<int, 3> v3 = v1 + v2;

		EXPECT_EQ(v3[0], 5);
		EXPECT_EQ(v3[1], 7);
		EXPECT_EQ(v3[2], 9);
	}

	TEST(math_vector, VectorSubtraction) {
		TVector<int, 3> v1(5, 6, 7);
		TVector<int, 3> v2(1, 2, 3);
		TVector<int, 3> v3 = v1 - v2;

		EXPECT_EQ(v3[0], 4);
		EXPECT_EQ(v3[1], 4);
		EXPECT_EQ(v3[2], 4);
	}

	TEST(math_vector, VectorMultiplicationByScalar) {
		TVector<int, 3> v(1, 2, 3);
		TVector<int, 3> v2 = v * 2;

		EXPECT_EQ(v2[0], 2);
		EXPECT_EQ(v2[1], 4);
		EXPECT_EQ(v2[2], 6);
	}

	TEST(math_vector, VectorDivisionByScalar) {
		TVector<int, 3> v(2, 4, 6);
		TVector<int, 3> v2 = v / 2;

		EXPECT_EQ(v2[0], 1);
		EXPECT_EQ(v2[1], 2);
		EXPECT_EQ(v2[2], 3);
	}

	// Comparison operation tests
	TEST(math_vector, AllBigger) {
		TVector<int, 3> v1(5, 6, 7);
		TVector<int, 3> v2(1, 2, 3);
		EXPECT_TRUE(v1.AllBigger(v2));
		EXPECT_FALSE(v2.AllBigger(v1));
	}

	TEST(math_vector, AllLess) {
		TVector<int, 3> v1(1, 2, 3);
		TVector<int, 3> v2(5, 6, 7);
		EXPECT_TRUE(v1.AllLess(v2));
		EXPECT_FALSE(v2.AllLess(v1));
	}

	TEST(math_vector, AllBiggerEqual) {
		TVector<int, 3> v1(5, 6, 7);
		TVector<int, 3> v2(5, 5, 7);
		EXPECT_TRUE(v1.AllBiggerEqual(v2));
		EXPECT_FALSE(v2.AllBiggerEqual(v1));
	}

	TEST(math_vector, AllLessEqual) {
		TVector<int, 3> v1(1, 2, 3);
		TVector<int, 3> v2(1, 3, 3);
		EXPECT_TRUE(v1.AllLessEqual(v2));
		EXPECT_FALSE(v2.AllLessEqual(v1));
	}

	TEST(math_vector, Equality) {
		TVector<int, 3> v1(1, 2, 3);
		TVector<int, 3> v2(1, 2, 3);
		TVector<int, 3> v3(3, 2, 1);

		EXPECT_TRUE(v1 == v2);
		EXPECT_FALSE(v1 == v3);
	}

	// Utility function tests
	TEST(math_vector, Space) {
		TVector<int, 2> v1(3, 4);
		EXPECT_EQ(v1.Space(), 12);

		TVector<int, 3> v2(2, 3, 4);
		EXPECT_EQ(v2.Space(), 24);
	}

	// Type conversion tests
	TEST(math_vector, TypeConversion) {
		TVector<int, 3> v1(1, 2, 3);
		TVector<double, 3> v2 = (TVector<double, 3>)v1;

		EXPECT_DOUBLE_EQ(v2[0], 1.0);
		EXPECT_DOUBLE_EQ(v2[1], 2.0);
		EXPECT_DOUBLE_EQ(v2[2], 3.0);
	}


	/*******************************************************************/

	// Test for element access operator
	TEST(math_matrix, ElementAccess)
	{
		TMatrix<int, 2, 2> m;
		m(0, 0) = 1;
		m(0, 1) = 2;
		m(1, 0) = 3;
		m(1, 1) = 4;

		EXPECT_EQ(m(0, 0), 1);
		EXPECT_EQ(m(0, 1), 2);
		EXPECT_EQ(m(1, 0), 3);
		EXPECT_EQ(m(1, 1), 4);
	}

	// Test for matrix addition
	TEST(math_matrix, Addition)
	{
		TMatrix<int, 2, 2> m1;
		TMatrix<int, 2, 2> m2;

		m1(0, 0) = 1; m1(0, 1) = 2;
		m1(1, 0) = 3; m1(1, 1) = 4;

		m2(0, 0) = 5; m2(0, 1) = 6;
		m2(1, 0) = 7; m2(1, 1) = 8;

		TMatrix<int, 2, 2> m3 = m1 + m2;

		EXPECT_EQ(m3(0, 0), 6);
		EXPECT_EQ(m3(0, 1), 8);
		EXPECT_EQ(m3(1, 0), 10);
		EXPECT_EQ(m3(1, 1), 12);
	}

	// Test for matrix subtraction
	TEST(math_matrix, Subtraction)
	{
		TMatrix<int, 2, 2> m1;
		TMatrix<int, 2, 2> m2;

		m1(0, 0) = 10; m1(0, 1) = 20;
		m1(1, 0) = 30; m1(1, 1) = 40;

		m2(0, 0) = 5; m2(0, 1) = 10;
		m2(1, 0) = 15; m2(1, 1) = 20;

		TMatrix<int, 2, 2> m3 = m1 - m2;

		EXPECT_EQ(m3(0, 0), 5);
		EXPECT_EQ(m3(0, 1), 10);
		EXPECT_EQ(m3(1, 0), 15);
		EXPECT_EQ(m3(1, 1), 20);
	}

	// Test for scalar multiplication
	TEST(math_matrix, ScalarMultiplication)
	{
		TMatrix<int, 2, 2> m;
		m(0, 0) = 1; m(0, 1) = 2;
		m(1, 0) = 3; m(1, 1) = 4;

		TMatrix<int, 2, 2> m2 = m * 2;

		EXPECT_EQ(m2(0, 0), 2);
		EXPECT_EQ(m2(0, 1), 4);
		EXPECT_EQ(m2(1, 0), 6);
		EXPECT_EQ(m2(1, 1), 8);
	}

	// Test for matrix transpose
	TEST(math_matrix, Transpose)
	{
		TMatrix<int, 2, 3> m;
		m(0, 0) = 1; m(0, 1) = 2; m(0, 2) = 3;
		m(1, 0) = 4; m(1, 1) = 5; m(1, 2) = 6;

		TMatrix<int, 3, 2> t = m.Transpose();

		EXPECT_EQ(t(0, 0), 1);
		EXPECT_EQ(t(1, 0), 2);
		EXPECT_EQ(t(2, 0), 3);
		EXPECT_EQ(t(0, 1), 4);
		EXPECT_EQ(t(1, 1), 5);
		EXPECT_EQ(t(2, 1), 6);
	}

	// Test for matrix-vector multiplication
	TEST(math_matrix, MatrixVectorMultiplication)
	{
		TMatrix<int, 2, 3> m;
		m(0, 0) = 1; m(0, 1) = 2; m(0, 2) = 3;
		m(1, 0) = 4; m(1, 1) = 5; m(1, 2) = 6;

		TVector<int, 3> v;
		v[0] = 1; v[1] = 2; v[2] = 3;

		TVector<int, 2> result = m * v;

		EXPECT_EQ(result[0], 14);  // 1*1 + 2*2 + 3*3 = 14
		EXPECT_EQ(result[1], 32);  // 4*1 + 5*2 + 6*3 = 32
	}

	// Test for matrix multiplication
	TEST(math_matrix, MatrixMultiplication)
	{
		TMatrix<int, 2, 3> m1;
		TMatrix<int, 3, 2> m2;

		// Fill m1 with 1, 2, 3 in first row and 4, 5, 6 in second row
		m1(0, 0) = 1; m1(0, 1) = 2; m1(0, 2) = 3;
		m1(1, 0) = 4; m1(1, 1) = 5; m1(1, 2) = 6;

		// Fill m2 with 7, 8 in first column, 9, 10 in second column, 11, 12 in third column
		m2(0, 0) = 7;  m2(0, 1) = 8;
		m2(1, 0) = 9;  m2(1, 1) = 10;
		m2(2, 0) = 11; m2(2, 1) = 12;

		TMatrix<int, 2, 2> result = m1 * m2;

		EXPECT_EQ(result(0, 0), 58);  // 1*7 + 2*9 + 3*11 = 58
		EXPECT_EQ(result(0, 1), 64);  // 1*8 + 2*10 + 3*12 = 64
		EXPECT_EQ(result(1, 0), 139); // 4*7 + 5*9 + 6*11 = 139
		EXPECT_EQ(result(1, 1), 154); // 4*8 + 5*10 + 6*12 = 154
	}

	// Test for translation matrix creation
	TEST(math_matrix, CreateTranslationMatrix)
	{
		TVector<int, 2> v;
		v[0] = 2;
		v[1] = 3;

		TMatrix<int, 3, 3> m = CreateTranslationMatrix(v);

		EXPECT_EQ(m(0, 0), 1);
		EXPECT_EQ(m(1, 1), 1);
		EXPECT_EQ(m(2, 2), 1);
		EXPECT_EQ(m(0, 2), 2);
		EXPECT_EQ(m(1, 2), 3);
		EXPECT_EQ(m(2, 0), 0);
		EXPECT_EQ(m(2, 1), 0);
	}

	TEST(math_matrix, CreateScalingMatrix)
	{
		TVector<int, 2> v;
		v[0] = 2;
		v[1] = 3;

		TMatrix<int, 3, 3> m = CreateScalingMatrix(v);

		EXPECT_EQ(m(0, 0), 2);
		EXPECT_EQ(m(0, 1), 0);
		EXPECT_EQ(m(0, 2), 0);
		EXPECT_EQ(m(1, 0), 0);
		EXPECT_EQ(m(1, 1), 3);
		EXPECT_EQ(m(1, 2), 0);
		EXPECT_EQ(m(2, 0), 0);
		EXPECT_EQ(m(2, 1), 0);
		EXPECT_EQ(m(2, 2), 1);
	}

	// Test for translation matrix creation
	TEST(math_matrix, Idenity)
	{
		auto m = TMatrix<int, 3, 3>::Identity();

		EXPECT_EQ(m(0, 0), 1);
		EXPECT_EQ(m(0, 1), 0);
		EXPECT_EQ(m(0, 2), 0);
		EXPECT_EQ(m(1, 0), 0);
		EXPECT_EQ(m(1, 1), 1);
		EXPECT_EQ(m(1, 2), 0);
		EXPECT_EQ(m(2, 0), 0);
		EXPECT_EQ(m(2, 1), 0);
		EXPECT_EQ(m(2, 2), 1);
	}

	TEST(math_matrix, CreateMirrorMatrix)
	{
		{
			TMatrix<int, 3, 3> m = CreateMirrorMatrix<int>(false, false);

			EXPECT_EQ(m(0, 0), 1);
			EXPECT_EQ(m(0, 1), 0);
			EXPECT_EQ(m(0, 2), 0);
			EXPECT_EQ(m(1, 0), 0);
			EXPECT_EQ(m(1, 1), 1);
			EXPECT_EQ(m(1, 2), 0);
			EXPECT_EQ(m(2, 0), 0);
			EXPECT_EQ(m(2, 1), 0);
			EXPECT_EQ(m(2, 2), 1);
		}

		{
			TMatrix<int, 3, 3> m = CreateMirrorMatrix<int>(true, false);

			EXPECT_EQ(m(0, 0), 1);
			EXPECT_EQ(m(0, 1), 0);
			EXPECT_EQ(m(0, 2), 0);
			EXPECT_EQ(m(1, 0), 0);
			EXPECT_EQ(m(1, 1), -1);
			EXPECT_EQ(m(1, 2), 0);
			EXPECT_EQ(m(2, 0), 0);
			EXPECT_EQ(m(2, 1), 0);
			EXPECT_EQ(m(2, 2), 1);
		}

		{
			TMatrix<int, 3, 3> m = CreateMirrorMatrix<int>(false, true);

			EXPECT_EQ(m(0, 0), -1);
			EXPECT_EQ(m(0, 1), 0);
			EXPECT_EQ(m(0, 2), 0);
			EXPECT_EQ(m(1, 0), 0);
			EXPECT_EQ(m(1, 1), 1);
			EXPECT_EQ(m(1, 2), 0);
			EXPECT_EQ(m(2, 0), 0);
			EXPECT_EQ(m(2, 1), 0);
			EXPECT_EQ(m(2, 2), 1);
		}
	}
}
