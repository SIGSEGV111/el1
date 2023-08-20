#pragma once

namespace el1::math
{
	template<typename T, unsigned n_dim>
	struct TVector
	{
		T v[n_dim];

		T& operator[](const unsigned index)
		{
			return v[index];
		}

		const T& operator[](const unsigned index) const
		{
			return v[index];
		}

		TVector& operator+=(const TVector& rhs)
		{
			for(unsigned i = 0; i < n_dim; i++)
				v[i] += rhs.v[i];
			return *this;
		}

		TVector& operator-=(const TVector& rhs)
		{
			for(unsigned i = 0; i < n_dim; i++)
				v[i] -= rhs.v[i];
			return *this;
		}

		TVector& operator*=(const T rhs)
		{
			for(unsigned i = 0; i < n_dim; i++)
				v[i] *= rhs;
			return *this;
		}

		TVector& operator/=(const T rhs)
		{
			for(unsigned i = 0; i < n_dim; i++)
				v[i] /= rhs;
			return *this;
		}

		TVector operator+(const TVector& rhs) const
		{
			TVector r = *this;
			r += rhs;
			return r;
		}

		TVector operator-(const TVector& rhs) const
		{
			TVector r = *this;
			r -= rhs;
			return r;
		}

		TVector operator*(const T rhs) const
		{
			TVector r = *this;
			r *= rhs;
			return r;
		}

		TVector operator/(const T rhs) const
		{
			TVector r = *this;
			r /= rhs;
			return r;
		}

		bool AllBigger(const TVector& rhs) const
		{
			for(unsigned i = 0; i < n_dim; i++)
				if(v[i] <= rhs[i])
					return false;
			return true;
		}

		bool AllLess(const TVector& rhs) const
		{
			for(unsigned i = 0; i < n_dim; i++)
				if(v[i] >= rhs[i])
					return false;
			return true;
		}

		bool AllBiggerEqual(const TVector& rhs) const
		{
			for(unsigned i = 0; i < n_dim; i++)
				if(v[i] < rhs[i])
					return false;
			return true;
		}

		bool AllLessEqual(const TVector& rhs) const
		{
			for(unsigned i = 0; i < n_dim; i++)
				if(v[i] > rhs[i])
					return false;
			return true;
		}

		bool operator==(const TVector& rhs) const
		{
			for(unsigned i = 0; i < n_dim; i++)
				if(v[i] == rhs[i])
					return false;
			return true;
		}

		TVector& operator=(TVector&&) = default;
		TVector& operator=(const TVector&) = default;

		TVector() = default;
		TVector(TVector&&) = default;
		TVector(const TVector&) = default;

		template<typename A>
		explicit operator TVector<A, n_dim>() const
		{
			TVector<A, n_dim> r;
			for(unsigned i = 0; i < n_dim; i++)
				r[i] = v[i];
			return r;
		}
	};
}
