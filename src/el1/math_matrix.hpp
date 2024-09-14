#pragma once

#include "def.hpp"
#include "io_types.hpp"
#include "math_vector.hpp"

namespace el1::math::matrix
{
	using namespace math::vector;

	template<typename T, unsigned rows, unsigned cols>
	struct matrix_t
	{
		T m[rows][cols];

		/**
		* @brief Access element at the given row and column index.
		* @param row The row index of the element to access.
		* @param col The column index of the element to access.
		* @return Reference to the element at the specified index.
		*/
		constexpr T& operator()(const unsigned row, const unsigned col);

		/**
		* @brief Access element at the given row and column index (const).
		* @param row The row index of the element to access.
		* @param col The column index of the element to access.
		* @return Const reference to the element at the specified index.
		*/
		constexpr const T& operator()(const unsigned row, const unsigned col) const;

		/**
		* @brief Adds another matrix to this matrix.
		* @param rhs The right-hand side matrix to add.
		* @return Reference to this matrix after addition.
		*/
		constexpr matrix_t& operator+=(const matrix_t& rhs);

		/**
		* @brief Subtracts another matrix from this matrix.
		* @param rhs The right-hand side matrix to subtract.
		* @return Reference to this matrix after subtraction.
		*/
		constexpr matrix_t& operator-=(const matrix_t& rhs);

		/**
		* @brief Multiplies this matrix by a scalar.
		* @param rhs The scalar value to multiply.
		* @return Reference to this matrix after multiplication.
		*/
		constexpr matrix_t& operator*=(const T rhs);

		/**
		* @brief Adds two matrices.
		* @param rhs The right-hand side matrix to add.
		* @return A new matrix resulting from the addition.
		*/
		constexpr matrix_t operator+(const matrix_t& rhs) const;

		/**
		* @brief Subtracts two matrices.
		* @param rhs The right-hand side matrix to subtract.
		* @return A new matrix resulting from the subtraction.
		*/
		constexpr matrix_t operator-(const matrix_t& rhs) const;

		/**
		* @brief Multiplies a matrix by a scalar.
		* @param rhs The scalar value to multiply.
		* @return A new matrix resulting from the multiplication.
		*/
		constexpr matrix_t operator*(const T rhs) const;

		/**
		* @brief Multiplies this matrix with a vector.
		* @param vec The vector to multiply.
		* @return A new vector resulting from the multiplication.
		*/
		constexpr vector_t<T, rows> operator*(const vector_t<T, cols>& vec) const;

		/**
		* @brief Multiplies two matrices.
		* @param rhs The right-hand side matrix to multiply.
		* @return A new matrix resulting from the multiplication.
		*/
		template<unsigned new_cols>
		constexpr matrix_t<T, rows, new_cols> operator*(const matrix_t<T, cols, new_cols>& rhs) const;

		/**
		* @brief Transposes the matrix (rows become columns and vice versa).
		* @return A new transposed matrix.
		*/
		constexpr matrix_t<T, cols, rows> Transpose() const;

		/**
		* @brief Creates a identity matrix
		* @return A new idenity matrix.
		*/
		static constexpr matrix_t Identity() EL_GETTER;

		matrix_t() = default;
		matrix_t(const matrix_t&) = default;
		matrix_t& operator=(const matrix_t&) = default;
	};

	template<typename T, unsigned n>
	matrix_t<T, n + 1, n + 1> CreateTranslationMatrix(const vector_t<T,n>& v)
	{
		matrix_t<T, n + 1, n + 1> m;
		for(unsigned r = 0; r < n + 1; r++)
		{
			for(unsigned c = 0; c < n; c++)
				m(r,c) = (r == c) ? 1 : 0;

			m(r,n) = v[r];
		}
		m(n,n) = 1;
		return m;
	}

	namespace
	{
		template<typename M, typename ... A>
		void CreateMirrorMatrixHelper(M& m, const unsigned r, const bool b);

		template<typename M, typename ... A>
		void CreateMirrorMatrixHelper(M& m, const unsigned r, const bool b, A ... a);
	}

	template<typename T, typename ... A>
	auto CreateMirrorMatrix(A ... a)
	{
		constexpr unsigned n = sizeof...(a) + 1;
		matrix_t<T, n, n> m;

		for(unsigned r = 0; r < n; r++)
			for(unsigned c = 0; c < n; c++)
				m(r,c) = 0;

		m(n-1,n-1) = 1;
		CreateMirrorMatrixHelper(m, n-2, a ...);
		return m;
	}

	template<typename T, unsigned n>
	auto CreateScalingMatrix(const vector_t<T,n>& v)
	{
		constexpr unsigned s = n + 1;
		matrix_t<T, s, s> m;

		for(unsigned r = 0; r < s; r++)
			for(unsigned c = 0; c < s; c++)
				m(r,c) = (r == c) ? (r < n ? v[r] : 1) : 0;

		return m;
	}

	/*******************************************************************/

	namespace
	{
		template<typename M, typename ... A>
		void CreateMirrorMatrixHelper(M& m, const unsigned r, const bool b)
		{
			m(r,r) = b ? -1 : 1;
		}

		template<typename M, typename ... A>
		void CreateMirrorMatrixHelper(M& m, const unsigned r, const bool b, A ... a)
		{
			CreateMirrorMatrixHelper(m, r, b);
			CreateMirrorMatrixHelper(m, r - 1, a ...);
		}
	}

	template<typename T, unsigned rows, unsigned cols>
	constexpr T& matrix_t<T, rows, cols>::operator()(const unsigned row, const unsigned col)
	{
		return m[row][col];
	}

	template<typename T, unsigned rows, unsigned cols>
	constexpr const T& matrix_t<T, rows, cols>::operator()(const unsigned row, const unsigned col) const
	{
		return m[row][col];
	}

	template<typename T, unsigned rows, unsigned cols>
	constexpr matrix_t<T, rows, cols>& matrix_t<T, rows, cols>::operator+=(const matrix_t& rhs)
	{
		for (unsigned i = 0; i < rows; ++i)
		{
			for (unsigned j = 0; j < cols; ++j)
			{
				m[i][j] += rhs.m[i][j];
			}
		}
		return *this;
	}

	template<typename T, unsigned rows, unsigned cols>
	constexpr matrix_t<T, rows, cols>& matrix_t<T, rows, cols>::operator-=(const matrix_t& rhs)
	{
		for (unsigned i = 0; i < rows; ++i)
		{
			for (unsigned j = 0; j < cols; ++j)
			{
				m[i][j] -= rhs.m[i][j];
			}
		}
		return *this;
	}

	template<typename T, unsigned rows, unsigned cols>
	constexpr matrix_t<T, rows, cols>& matrix_t<T, rows, cols>::operator*=(const T rhs)
	{
		for (unsigned i = 0; i < rows; ++i)
		{
			for (unsigned j = 0; j < cols; ++j)
			{
				m[i][j] *= rhs;
			}
		}
		return *this;
	}

	template<typename T, unsigned rows, unsigned cols>
	constexpr matrix_t<T, rows, cols> matrix_t<T, rows, cols>::operator+(const matrix_t& rhs) const
	{
		matrix_t result;
		for (unsigned i = 0; i < rows; ++i)
		{
			for (unsigned j = 0; j < cols; ++j)
			{
				result.m[i][j] = m[i][j] + rhs.m[i][j];
			}
		}
		return result;
	}

	template<typename T, unsigned rows, unsigned cols>
	constexpr matrix_t<T, rows, cols> matrix_t<T, rows, cols>::operator-(const matrix_t& rhs) const
	{
		matrix_t result;
		for (unsigned i = 0; i < rows; ++i)
		{
			for (unsigned j = 0; j < cols; ++j)
			{
				result.m[i][j] = m[i][j] - rhs.m[i][j];
			}
		}
		return result;
	}

	template<typename T, unsigned rows, unsigned cols>
	constexpr matrix_t<T, rows, cols> matrix_t<T, rows, cols>::operator*(const T rhs) const
	{
		matrix_t result;
		for (unsigned i = 0; i < rows; ++i)
		{
			for (unsigned j = 0; j < cols; ++j)
			{
				result.m[i][j] = m[i][j] * rhs;
			}
		}
		return result;
	}

	template<typename T, unsigned rows, unsigned cols>
	constexpr vector_t<T, rows> matrix_t<T, rows, cols>::operator*(const vector_t<T, cols>& vec) const
	{
		vector_t<T, rows> result;
		for (unsigned i = 0; i < rows; ++i)
		{
			result[i] = 0;
			for (unsigned j = 0; j < cols; ++j)
			{
				result[i] += m[i][j] * vec[j];
			}
		}
		return result;
	}

	template<typename T, unsigned rows, unsigned cols>
	template<unsigned new_cols>
	constexpr matrix_t<T, rows, new_cols> matrix_t<T, rows, cols>::operator*(const matrix_t<T, cols, new_cols>& rhs) const
	{
		matrix_t<T, rows, new_cols> result;
		for (unsigned i = 0; i < rows; ++i)
		{
			for (unsigned j = 0; j < new_cols; ++j)
			{
				result.m[i][j] = 0;
				for (unsigned k = 0; k < cols; ++k)
				{
					result.m[i][j] += m[i][k] * rhs.m[k][j];
				}
			}
		}
		return result;
	}

	template<typename T, unsigned rows, unsigned cols>
	constexpr matrix_t<T, cols, rows> matrix_t<T, rows, cols>::Transpose() const
	{
		matrix_t<T, cols, rows> result;
		for (unsigned i = 0; i < rows; ++i)
		{
			for (unsigned j = 0; j < cols; ++j)
			{
				result.m[j][i] = m[i][j];
			}
		}
		return result;
	}

	template<typename T, unsigned rows, unsigned cols>
	constexpr matrix_t<T, rows, cols> matrix_t<T, rows, cols>::Identity()
	{
		matrix_t<T, rows, cols> result;
		for (unsigned i = 0; i < rows; ++i)
		{
			for (unsigned j = 0; j < cols; ++j)
			{
				result.m[j][i] = (i == j) ? 1 : 0;
			}
		}
		return result;
	}
}
