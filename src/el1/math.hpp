#pragma once

#include "io_types.hpp"

namespace el1::math
{
	enum class ERoundingMode : io::types::u8_t
	{
		TO_NEAREST,		// rounds up when the digit is >=(base/2) and down otherwise
		TOWARDS_ZERO,	// rounds up when the number is negative and down when the number is positive
		AWAY_FROM_ZERO,	// the opposite of TOWARDS_ZERO
		DOWNWARD,		// always rounds down (negative numbers round away from zero, positive numbers round towards zero)
		UPWARD			// always rounds up (negative numbers round towards zero, positive numbers away from zero)
	};

	template<typename T, unsigned n_dim>
	struct vector_t
	{
		T v[n_dim];

		constexpr T& operator[](const unsigned index)
		{
			return v[index];
		}

		constexpr const T& operator[](const unsigned index) const
		{
			return v[index];
		}

		constexpr vector_t& operator+=(const vector_t& rhs)
		{
			for(unsigned i = 0; i < n_dim; i++)
				v[i] += rhs.v[i];
			return *this;
		}

		constexpr vector_t& operator-=(const vector_t& rhs)
		{
			for(unsigned i = 0; i < n_dim; i++)
				v[i] -= rhs.v[i];
			return *this;
		}

		constexpr vector_t& operator*=(const T rhs)
		{
			for(unsigned i = 0; i < n_dim; i++)
				v[i] *= rhs;
			return *this;
		}

		constexpr vector_t& operator/=(const T rhs)
		{
			for(unsigned i = 0; i < n_dim; i++)
				v[i] /= rhs;
			return *this;
		}

		constexpr vector_t operator+(const vector_t& rhs) const
		{
			vector_t r = *this;
			r += rhs;
			return r;
		}

		constexpr vector_t operator-(const vector_t& rhs) const
		{
			vector_t r = *this;
			r -= rhs;
			return r;
		}

		constexpr vector_t operator*(const T rhs) const
		{
			vector_t r = *this;
			r *= rhs;
			return r;
		}

		constexpr vector_t operator/(const T rhs) const
		{
			vector_t r = *this;
			r /= rhs;
			return r;
		}

		constexpr bool AllBigger(const vector_t& rhs) const
		{
			for(unsigned i = 0; i < n_dim; i++)
				if(v[i] <= rhs[i])
					return false;
			return true;
		}

		constexpr bool AllLess(const vector_t& rhs) const
		{
			for(unsigned i = 0; i < n_dim; i++)
				if(v[i] >= rhs[i])
					return false;
			return true;
		}

		constexpr bool AllBiggerEqual(const vector_t& rhs) const
		{
			for(unsigned i = 0; i < n_dim; i++)
				if(v[i] < rhs[i])
					return false;
			return true;
		}

		constexpr bool AllLessEqual(const vector_t& rhs) const
		{
			for(unsigned i = 0; i < n_dim; i++)
				if(v[i] > rhs[i])
					return false;
			return true;
		}

		constexpr bool operator==(const vector_t& rhs) const
		{
			for(unsigned i = 0; i < n_dim; i++)
				if(v[i] == rhs[i])
					return false;
			return true;
		}

		vector_t& operator=(vector_t&&) = default;
		vector_t& operator=(const vector_t&) = default;

		// computes the space defined by this vector
		// generalization of area (2d) or volume (3d)
		constexpr T Space() const EL_GETTER
		{
			T s = v[0];
			for(unsigned i = 1; i < n_dim; i++)
				s *= v[i];
			return s;
		}

		template<typename A>
		explicit operator vector_t<A, n_dim>() const
		{
			vector_t<A, n_dim> r;
			for(unsigned i = 0; i < n_dim; i++)
				r[i] = v[i];
			return r;
		}

		template<typename TT>
		constexpr vector_t(const TT (&vv)[n_dim])
		{
			for(unsigned i = 0; i < n_dim; i++)
				v[i] = vv[i];
		}

		vector_t() = default;
		vector_t(vector_t&&) = default;
		vector_t(const vector_t&) = default;
	};

	using v2f_t = vector_t<float, 2>;
	using v3f_t = vector_t<float, 3>;
}
