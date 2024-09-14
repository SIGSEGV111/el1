#pragma once

#include "def.hpp"
#include "io_types.hpp"
#include <math.h>
#include <type_traits>

namespace el1::math::vector
{
	template<typename T, unsigned n_dim>
	struct vector_t
	{
		T v[n_dim];

		/**
		* @brief Access element at the given index.
		* @param index The index of the element to access.
		* @return Reference to the element at the specified index.
		*/
		constexpr T& operator[](const unsigned index);

		/**
		* @brief Access element at the given index (const).
		* @param index The index of the element to access.
		* @return Const reference to the element at the specified index.
		*/
		constexpr const T& operator[](const unsigned index) const;

		/**
		* @brief Adds another vector to this vector.
		* @param rhs The right-hand side vector to add.
		* @return Reference to this vector after addition.
		*/
		constexpr vector_t& operator+=(const vector_t& rhs);

		/**
		* @brief Subtracts another vector from this vector.
		* @param rhs The right-hand side vector to subtract.
		* @return Reference to this vector after subtraction.
		*/
		constexpr vector_t& operator-=(const vector_t& rhs);

		/**
		* @brief Multiplies this vector by a scalar.
		* @param rhs The scalar value to multiply.
		* @return Reference to this vector after multiplication.
		*/
		constexpr vector_t& operator*=(const T rhs);

		/**
		* @brief Divides this vector by a scalar.
		* @param rhs The scalar value to divide by.
		* @return Reference to this vector after division.
		*/
		constexpr vector_t& operator/=(const T rhs);

		/**
		* @brief Divides this vector by a scalar.
		* @param rhs The scalar value to divide by.
		* @return Reference to this vector after division.
		*/
		constexpr vector_t& operator%=(const T rhs);

		/**
		* @brief Adds two vectors.
		* @param rhs The right-hand side vector to add.
		* @return A new vector resulting from the addition.
		*/
		constexpr vector_t operator+(const vector_t& rhs) const;

		/**
		* @brief Subtracts two vectors.
		* @param rhs The right-hand side vector to subtract.
		* @return A new vector resulting from the subtraction.
		*/
		constexpr vector_t operator-(const vector_t& rhs) const;

		/**
		* @brief Multiplies a vector by a scalar.
		* @param rhs The scalar value to multiply.
		* @return A new vector resulting from the multiplication.
		*/
		constexpr vector_t operator*(const T rhs) const;

		/**
		* @brief Divides a vector by a scalar.
		* @param rhs The scalar value to divide by.
		* @return A new vector resulting from the division.
		*/
		constexpr vector_t operator/(const T rhs) const;

		/**
		* @brief Divides a vector by a scalar.
		* @param rhs The scalar value to divide by.
		* @return A new vector resulting from the division.
		*/
		constexpr vector_t operator%(const T rhs) const;

		/**
		* @brief Checks if all components of this vector are greater than those of another vector.
		* @param rhs The right-hand side vector to compare.
		* @return True if all components of this vector are greater, false otherwise.
		*/
		constexpr bool AllBigger(const vector_t& rhs) const;

		/**
		* @brief Checks if all components of this vector are less than those of another vector.
		* @param rhs The right-hand side vector to compare.
		* @return True if all components of this vector are less, false otherwise.
		*/
		constexpr bool AllLess(const vector_t& rhs) const;

		/**
		* @brief Checks if all components of this vector are greater than or equal to those of another vector.
		* @param rhs The right-hand side vector to compare.
		* @return True if all components of this vector are greater than or equal, false otherwise.
		*/
		constexpr bool AllBiggerEqual(const vector_t& rhs) const;

		/**
		* @brief Checks if all components of this vector are less than or equal to those of another vector.
		* @param rhs The right-hand side vector to compare.
		* @return True if all components of this vector are less than or equal, false otherwise.
		*/
		constexpr bool AllLessEqual(const vector_t& rhs) const;

		/**
		* @brief Checks if two vectors are equal.
		* @param rhs The right-hand side vector to compare.
		* @return True if all components of both vectors are equal, false otherwise.
		*/
		constexpr bool operator==(const vector_t& rhs) const;

		vector_t& operator=(vector_t&&) = default;
		vector_t& operator=(const vector_t&) = default;

		/**
		* @brief Computes the "space" defined by this vector, which is a generalization of area (2D) or volume (3D).
		* @return The computed space value.
		*/
		constexpr T Space() const EL_GETTER;

		/**
		* @brief Computes the length of the vector.
		* @return The computed length value.
		*/
		template<typename R = T>
		constexpr R Magnitude() const EL_GETTER;

		/**
		* @brief Computes the absolute distance between two vectors
		* @return The computed distance.
		*/
		template<typename R = T>
		constexpr R Distance(const vector_t& rhs) const EL_GETTER;

		/**
		* Calculates the element-wise absolute difference between this vector and another vector.
		*
		* @param rhs The vector to compare against.
		* @return A new vector where each element i is the absolute value of the difference between
		*         element i of this vector and element i of the vector rhs.
		*/
		constexpr vector_t AbsoluteDifference(const vector_t& rhs) const EL_GETTER;

		/**
		* @brief Conversion operator to a different vector type.
		* @tparam A The new type for the vector elements.
		* @return A new vector with elements converted to type A.
		*/
		template<typename A>
		explicit operator vector_t<A, n_dim>() const;

		/**
		* @brief Constructor to initialize vector with an array.
		* @tparam TT The type of the array elements.
		* @param vv The array of elements.
		*/
		template<typename TT>
		constexpr vector_t(const TT (&vv)[n_dim]);

		/**
		* @brief Variadic template constructor to initialize vector with multiple arguments.
		* @tparam A The types of the arguments.
		* @param a The arguments used to initialize the vector.
		*/
		template <typename... A>
		vector_t(A ... a);

		vector_t();
		vector_t(vector_t&&) = default;
		vector_t(const vector_t&) = default;

		template<typename TT>
		explicit vector_t(const vector_t<TT, n_dim>&);
	};

	/*******************************************************************/

	template<typename T, unsigned n_dim>
	constexpr T& vector_t<T, n_dim>::operator[](const unsigned index)
	{
		return v[index];
	}

	template<typename T, unsigned n_dim>
	constexpr const T& vector_t<T, n_dim>::operator[](const unsigned index) const
	{
		return v[index];
	}

	template<typename T, unsigned n_dim>
	constexpr vector_t<T, n_dim>& vector_t<T, n_dim>::operator+=(const vector_t& rhs)
	{
		for(unsigned i = 0; i < n_dim; i++)
			v[i] += rhs.v[i];
		return *this;
	}

	template<typename T, unsigned n_dim>
	constexpr vector_t<T, n_dim>& vector_t<T, n_dim>::operator-=(const vector_t& rhs)
	{
		for(unsigned i = 0; i < n_dim; i++)
			v[i] -= rhs.v[i];
		return *this;
	}

	template<typename T, unsigned n_dim>
	constexpr vector_t<T, n_dim>& vector_t<T, n_dim>::operator*=(const T rhs)
	{
		for(unsigned i = 0; i < n_dim; i++)
			v[i] *= rhs;
		return *this;
	}

	template<typename T, unsigned n_dim>
	constexpr vector_t<T, n_dim>& vector_t<T, n_dim>::operator/=(const T rhs)
	{
		for(unsigned i = 0; i < n_dim; i++)
			v[i] /= rhs;
		return *this;
	}

	template<typename T, unsigned n_dim>
	constexpr vector_t<T, n_dim>& vector_t<T, n_dim>::operator%=(const T rhs)
	{
		for(unsigned i = 0; i < n_dim; i++)
			v[i] %= rhs;
		return *this;
	}

	template<typename T, unsigned n_dim>
	constexpr vector_t<T, n_dim> vector_t<T, n_dim>::operator+(const vector_t& rhs) const
	{
		vector_t r = *this;
		r += rhs;
		return r;
	}

	template<typename T, unsigned n_dim>
	constexpr vector_t<T, n_dim> vector_t<T, n_dim>::operator-(const vector_t& rhs) const
	{
		vector_t r = *this;
		r -= rhs;
		return r;
	}

	template<typename T, unsigned n_dim>
	constexpr vector_t<T, n_dim> vector_t<T, n_dim>::operator*(const T rhs) const
	{
		vector_t r = *this;
		r *= rhs;
		return r;
	}

	template<typename T, unsigned n_dim>
	constexpr vector_t<T, n_dim> vector_t<T, n_dim>::operator/(const T rhs) const
	{
		vector_t r = *this;
		r /= rhs;
		return r;
	}

	template<typename T, unsigned n_dim>
	constexpr vector_t<T, n_dim> vector_t<T, n_dim>::operator%(const T rhs) const
	{
		vector_t r = *this;
		r %= rhs;
		return r;
	}

	template<typename T, unsigned n_dim>
	constexpr bool vector_t<T, n_dim>::AllBigger(const vector_t& rhs) const
	{
		for(unsigned i = 0; i < n_dim; i++)
			if(v[i] <= rhs[i])
				return false;
		return true;
	}

	template<typename T, unsigned n_dim>
	constexpr bool vector_t<T, n_dim>::AllLess(const vector_t& rhs) const
	{
		for(unsigned i = 0; i < n_dim; i++)
			if(v[i] >= rhs[i])
				return false;
		return true;
	}

	template<typename T, unsigned n_dim>
	constexpr bool vector_t<T, n_dim>::AllBiggerEqual(const vector_t& rhs) const
	{
		for(unsigned i = 0; i < n_dim; i++)
			if(v[i] < rhs[i])
				return false;
		return true;
	}

	template<typename T, unsigned n_dim>
	constexpr bool vector_t<T, n_dim>::AllLessEqual(const vector_t& rhs) const
	{
		for(unsigned i = 0; i < n_dim; i++)
			if(v[i] > rhs[i])
				return false;
		return true;
	}

	template<typename T, unsigned n_dim>
	constexpr bool vector_t<T, n_dim>::operator==(const vector_t& rhs) const
	{
		for(unsigned i = 0; i < n_dim; i++)
			if(v[i] != rhs[i])
				return false;
		return true;
	}

	template<typename T, unsigned n_dim>
	constexpr T vector_t<T, n_dim>::Space() const
	{
		T s = v[0];
		for(unsigned i = 1; i < n_dim; i++)
			s *= v[i];
		return s;
	}

	template<typename T, unsigned n_dim>
	template<typename R>
	constexpr R vector_t<T, n_dim>::Magnitude() const
	{
		R l = 0;
		for(unsigned i = 0; i < n_dim; i++)
			l += ((R)v[i] * (R)v[i]);

		if constexpr (std::is_same_v<R, float>)
			return sqrtf(l);

		if constexpr (std::is_same_v<R, long double>)
			return sqrtl(l);

		return sqrt(l);
	}

	template<typename T, unsigned n_dim>
	template<typename R>
	constexpr R vector_t<T, n_dim>::Distance(const vector_t<T, n_dim>& rhs) const
	{
		return (*this - rhs).template Magnitude<R>();
	}

	template<typename T, unsigned n_dim>
	constexpr vector_t<T, n_dim> vector_t<T, n_dim>::AbsoluteDifference(const vector_t& rhs) const
	{
		vector_t dist;
		for(unsigned i = 0; i < n_dim; i++)
			dist[i] = (v[i] >= rhs[i]) ? (v[i] - rhs[i]) : (rhs[i] - v[i]);
		return dist;
	}

	template<typename T, unsigned n_dim>
	template<typename A>
	vector_t<T, n_dim>::operator vector_t<A, n_dim>() const
	{
		vector_t<A, n_dim> r;
		for(unsigned i = 0; i < n_dim; i++)
			r[i] = v[i];
		return r;
	}

	template<typename T, unsigned n_dim>
	template<typename TT>
	constexpr vector_t<T, n_dim>::vector_t(const TT (&vv)[n_dim])
	{
		for(unsigned i = 0; i < n_dim; i++)
			v[i] = vv[i];
	}

	template<typename T, unsigned n_dim>
	vector_t<T, n_dim>::vector_t()
	{
		for(unsigned i = 0; i < n_dim; i++)
			v[i] = 0;
	}

	template<typename T, unsigned n_dim>
	template <typename... A>
	vector_t<T, n_dim>::vector_t(A ... a) : v{static_cast<T>(a)...}
	{
		static_assert(sizeof...(A) == n_dim, "Constructor requires exactly n_dim arguments");
	}

	template<typename T, unsigned n_dim>
	template<typename TT>
	vector_t<T, n_dim>::vector_t(const vector_t<TT, n_dim>& r)
	{
		for(unsigned i = 0; i < n_dim; i++)
			v[i] = static_cast<TT>(r.v[i]);
	}
}

